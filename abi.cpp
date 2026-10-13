#include "abi.h"
#include "abi_profiles.h"
#include "compatibility.h"
#include <Windows.h>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace {
template<class T> T Read(std::span<const unsigned char> b,size_t p) {
    if(p>b.size()||sizeof(T)>b.size()-p)throw std::runtime_error("ABI bounds");
    T v;memcpy(&v,b.data()+p,sizeof(v));return v;
}
}
bool VerifyABI(const std::wstring& path,std::span<const unsigned char> live,
               uintptr_t base,std::vector<MetadataBinding>& bindings) {
    bindings.clear();
    std::ifstream file(std::filesystem::path(path),std::ios::binary|std::ios::ate);
    if(!file||file.tellg()<0||file.tellg()>256*1024*1024)return false;
    std::vector<unsigned char> raw(static_cast<size_t>(file.tellg()));file.seekg(0);
    if(!file.read(reinterpret_cast<char*>(raw.data()),raw.size()))return false;
    const auto dos=Read<IMAGE_DOS_HEADER>(raw,0);
    if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0)return false;
    const auto nt=Read<IMAGE_NT_HEADERS64>(raw,dos.e_lfanew);
    if(nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||
       nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt.OptionalHeader.SizeOfImage!=ABISizeOfImage||
       nt.OptionalHeader.NumberOfRvaAndSizes<16||nt.FileHeader.NumberOfSections!=std::size(ABISections)+1)return false;
    std::vector<unsigned char> reference(ABISizeOfImage);
    std::vector<IMAGE_SECTION_HEADER> sections;
    const size_t table=size_t(dos.e_lfanew)+24+nt.FileHeader.SizeOfOptionalHeader;
    unsigned matched=0;
    for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i) {
        const auto s=Read<IMAGE_SECTION_HEADER>(raw,table+i*sizeof(IMAGE_SECTION_HEADER));
        if(s.PointerToRawData>raw.size()||s.SizeOfRawData>raw.size()-s.PointerToRawData||
           s.VirtualAddress>reference.size()||s.SizeOfRawData>reference.size()-s.VirtualAddress)return false;
        if(memcmp(s.Name,".rsrc\0\0\0",8)) {
            if(matched>=std::size(ABISections))return false;
            const auto& p=ABISections[matched++];std::string hash;
            if(s.VirtualAddress!=p.rva||s.SizeOfRawData!=p.size||s.Misc.VirtualSize!=p.virtualSize||s.Characteristics!=p.flags||
               !HashBytes(std::span(raw).subspan(s.PointerToRawData,s.SizeOfRawData),hash)||hash!=p.hash)return false;
        }
        memcpy(reference.data()+s.VirtualAddress,raw.data()+s.PointerToRawData,s.SizeOfRawData);
        sections.push_back(s);
    }
    if(matched!=std::size(ABISections))return false;
    // Normalize relocation addresses in a private reference, never in the game.
    const uintptr_t delta=base-nt.OptionalHeader.ImageBase;
    const auto reloc=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if(reloc.VirtualAddress>reference.size()||reloc.Size>reference.size()-reloc.VirtualAddress)return false;
    size_t cursor=reloc.VirtualAddress,end=cursor+reloc.Size;
    std::vector<uint32_t> pointerSlots;
    while(cursor<end) {
        auto b=Read<IMAGE_BASE_RELOCATION>(reference,cursor);
        if(b.SizeOfBlock<8||(b.SizeOfBlock%2)||b.SizeOfBlock>end-cursor)return false;
        for(size_t o=8;o<b.SizeOfBlock;o+=2) {
            auto e=Read<uint16_t>(reference,cursor+o);unsigned type=e>>12;
            if(!type)continue;if(type!=IMAGE_REL_BASED_DIR64)return false;
            size_t pos=size_t(b.VirtualAddress)+(e&0xfff);
            auto value=Read<uint64_t>(reference,pos)+delta;memcpy(reference.data()+pos,&value,8);
            pointerSlots.push_back(static_cast<uint32_t>(pos));
        }
        cursor+=b.SizeOfBlock;
    }
    std::sort(pointerSlots.begin(),pointerSlots.end());
    auto executable=[&](uintptr_t address){
        if(address<base)return false;const auto rva=address-base;
        for(auto s:sections)if((s.Characteristics&IMAGE_SCN_MEM_EXECUTE)&&rva>=s.VirtualAddress&&rva-s.VirtualAddress<s.Misc.VirtualSize)return true;
        return false;
    };
    if(!live.empty()) {
        if(live.size()!=reference.size())return false;
        for(auto s:sections)if(s.Characteristics&IMAGE_SCN_MEM_EXECUTE) {
            if(s.Misc.VirtualSize>s.SizeOfRawData||memcmp(live.data()+s.VirtualAddress,reference.data()+s.VirtualAddress,s.Misc.VirtualSize))return false;
        }
    }
    // Whitelist definition-interface vtables whose RTTI offset is exactly +0x30.
    // A random in-image function is never accepted as a metadata getter.
    for(auto s:sections)if(!memcmp(s.Name,".rdata\0\0",8)) {
        size_t start=s.VirtualAddress,endData=start+s.SizeOfRawData;
        for(size_t p=start+8;p+40<=endData;p+=8) {
            if(!std::binary_search(pointerSlots.begin(),pointerSlots.end(),p-8)||
               !std::binary_search(pointerSlots.begin(),pointerSlots.end(),p+32))continue;
            uintptr_t col=Read<uint64_t>(reference,p-8);
            if(col<base+start||col>base+endData-24)continue;
            size_t c=col-base;
            if(Read<uint32_t>(reference,c)!=1||Read<uint32_t>(reference,c+4)!=0x30||
               Read<uint32_t>(reference,c+8)!=0||Read<uint32_t>(reference,c+20)!=c)continue;
            uintptr_t getter=Read<uint64_t>(reference,p+32);
            std::string getterHash;
            if(executable(getter)&&getter-base+9<=reference.size()&&
               HashBytes(std::span(reference).subspan(getter-base,9),getterHash)&&getterHash==ABIMacroGetterHash)
                bindings.push_back({base+p,getter});
        }
    }
    return !bindings.empty();
}
