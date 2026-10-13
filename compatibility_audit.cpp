#include "compatibility.h"
#include "compatibility_profiles.h"
#include "abi.h"
#include <filesystem>
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <stdexcept>

template<class T>T read(const std::vector<unsigned char>& d,size_t p){
    if(p>d.size()||sizeof(T)>d.size()-p)throw std::runtime_error("truncated PE");
    T t{};memcpy(&t,d.data()+p,sizeof(t));return t;
}
int main(int argc,char** argv){try{
    if(argc!=2&&argc!=3)return 2;
    std::ifstream f(argv[1],std::ios::binary);
    if(!f)throw std::runtime_error("cannot open file");
    std::vector<unsigned char> raw((std::istreambuf_iterator<char>(f)),{});
    auto dos=read<IMAGE_DOS_HEADER>(raw,0);
    if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0)throw std::runtime_error("DOS header");
    auto nt=read<IMAGE_NT_HEADERS64>(raw,dos.e_lfanew);
    if(nt.Signature!=IMAGE_NT_SIGNATURE||nt.OptionalHeader.SizeOfImage>512*1024*1024||nt.OptionalHeader.SizeOfHeaders>raw.size()||nt.OptionalHeader.SizeOfHeaders>nt.OptionalHeader.SizeOfImage)throw std::runtime_error("PE bounds");
    std::vector<unsigned char> image(nt.OptionalHeader.SizeOfImage);
    memcpy(image.data(),raw.data(),nt.OptionalHeader.SizeOfHeaders);
    size_t table=size_t(dos.e_lfanew)+24+nt.FileHeader.SizeOfOptionalHeader;
    for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i){
        auto s=read<IMAGE_SECTION_HEADER>(raw,table+i*sizeof(IMAGE_SECTION_HEADER));
        if(s.PointerToRawData>raw.size()||s.SizeOfRawData>raw.size()-s.PointerToRawData||s.VirtualAddress>image.size()||s.SizeOfRawData>image.size()-s.VirtualAddress)throw std::runtime_error("section bounds");
        memcpy(image.data()+s.VirtualAddress,raw.data()+s.PointerToRawData,s.SizeOfRawData);
    }
    if(argc==3&&(std::string(argv[2])=="--abi"||std::string(argv[2])=="--abi-aslr"||std::string(argv[2])=="--abi-live-change")) {
        uintptr_t base=nt.OptionalHeader.ImageBase;
        if(std::string(argv[2])=="--abi-aslr") {
            const uint64_t delta=0x112340000ull;base+=delta;
            const auto reloc=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
            for(size_t cursor=reloc.VirtualAddress;cursor<size_t(reloc.VirtualAddress)+reloc.Size;) {
                auto block=read<IMAGE_BASE_RELOCATION>(image,cursor);
                if(block.SizeOfBlock<8||block.SizeOfBlock%2||block.SizeOfBlock>size_t(reloc.VirtualAddress)+reloc.Size-cursor)throw std::runtime_error("relocation block");
                for(size_t i=8;i<block.SizeOfBlock;i+=2) {
                    auto entry=read<uint16_t>(image,cursor+i);
                    if(!(entry>>12))continue;
                    if((entry>>12)!=IMAGE_REL_BASED_DIR64)throw std::runtime_error("relocation kind");
                    size_t target=size_t(block.VirtualAddress)+(entry&0xfff);
                    auto value=read<uint64_t>(image,target)+delta;memcpy(image.data()+target,&value,8);
                }
                cursor+=block.SizeOfBlock;
            }
        }
        if(std::string(argv[2])=="--abi-live-change")image.at(0x1000)^=1;
        std::vector<MetadataBinding> bindings;
        bool accepted=VerifyABI(std::filesystem::path(argv[1]).wstring(),image,base,bindings);
        std::cout<<"{\"accepted\":"<<(accepted?"true":"false")<<",\"metadata_tables\":"<<bindings.size()<<"}\n";return 0;
    }
    std::cout<<"[";bool comma=false;
    for(const auto& p:Profiles){
        auto m=FindFunction(image,p);
        if(comma)std::cout<<",";comma=true;
        std::cout<<"{\"name\":\""<<p.name<<"\",\"matches\":"<<m.matches<<",\"entry\":"<<m.entry<<",\"hook\":"<<m.hook<<"}";
    }
    std::cout<<"]\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}}
