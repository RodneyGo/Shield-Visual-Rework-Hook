#include "compatibility.h"
#include <Windows.h>
#include <bcrypt.h>
#include <cstring>
#include <limits>

bool HashBytes(std::span<const unsigned char> bytes, std::string& result) {
    result.clear();
    if(bytes.size()>std::numeric_limits<ULONG>::max()) return false;
    BCRYPT_ALG_HANDLE alg=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
    unsigned char digest[32]{};
    bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if(ok)ok=BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)>=0;
    if(ok)ok=BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),0)>=0;
    if(ok)ok=BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
    if(hash)BCryptDestroyHash(hash);
    if(alg)BCryptCloseAlgorithmProvider(alg,0);
    if(ok){const char* hex="0123456789abcdef";for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}}
    return ok;
}
namespace {
bool range(size_t size,size_t offset,size_t count){return offset<=size && count<=size-offset;}
template<class T> bool read(std::span<const unsigned char> image,size_t offset,T& value){
    if(!range(image.size(),offset,sizeof(T)))return false;
    std::memcpy(&value,image.data()+offset,sizeof(T));return true;
}
}
FunctionMatch FindFunction(std::span<const unsigned char> image,const FunctionProfile& p){
    FunctionMatch result;
    IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
    if(!read(image,0,dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0)return result;
    const size_t pe=static_cast<size_t>(dos.e_lfanew);
    if(!read(image,pe,nt)||nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return result;
    if(nt.FileHeader.SizeOfOptionalHeader<sizeof(IMAGE_OPTIONAL_HEADER64)||nt.OptionalHeader.NumberOfRvaAndSizes<=IMAGE_DIRECTORY_ENTRY_EXCEPTION||nt.OptionalHeader.SizeOfImage!=image.size())return result;
    const auto directory=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if(!directory.Size||directory.Size%sizeof(RUNTIME_FUNCTION)||!range(image.size(),directory.VirtualAddress,directory.Size)||p.hookOffset>=p.length)return result;
    const size_t sections=pe+24+nt.FileHeader.SizeOfOptionalHeader;
    if(!range(image.size(),sections,size_t(nt.FileHeader.NumberOfSections)*sizeof(IMAGE_SECTION_HEADER)))return result;
    for(size_t pos=directory.VirtualAddress;pos<size_t(directory.VirtualAddress)+directory.Size;pos+=sizeof(RUNTIME_FUNCTION)){
        RUNTIME_FUNCTION f{};read(image,pos,f);
        if(f.EndAddress<f.BeginAddress||f.EndAddress-f.BeginAddress!=p.length||!range(image.size(),f.BeginAddress,p.length)||!range(image.size(),f.UnwindData,p.unwindSize))continue;
        bool executable=false;
        for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i){
            IMAGE_SECTION_HEADER s{};read(image,sections+i*sizeof(s),s);
            if((s.Characteristics&IMAGE_SCN_MEM_EXECUTE)&&f.BeginAddress>=s.VirtualAddress&&uint64_t(f.EndAddress)<=uint64_t(s.VirtualAddress)+s.Misc.VirtualSize)executable=true;
        }
        if(!executable)continue;
        std::string hash;
        if(!HashBytes(image.subspan(f.BeginAddress,p.length),hash)||hash!=p.codeHash)continue;
        if(!HashBytes(image.subspan(f.UnwindData,p.unwindSize),hash)||hash!=p.unwindHash)continue;
        ++result.matches;result.entry=f.BeginAddress;result.hook=f.BeginAddress+p.hookOffset;
    }
    if(!result.unique()){result.entry=0;result.hook=0;}
    return result;
}
