#include "compatibility.h"
#include "compatibility_profiles.h"
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
    if(argc!=2)return 2;
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
    std::cout<<"[";bool comma=false;
    for(const auto& p:Profiles){
        auto m=FindFunction(image,p);
        if(comma)std::cout<<",";comma=true;
        std::cout<<"{\"name\":\""<<p.name<<"\",\"matches\":"<<m.matches<<",\"entry\":"<<m.entry<<",\"hook\":"<<m.hook<<"}";
    }
    std::cout<<"]\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 2;}}
