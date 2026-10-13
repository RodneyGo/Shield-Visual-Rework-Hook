// Regression checks in this process only. No game hooks are installed.
#include "probe.cpp"
#include <cassert>
#include <cstdlib>
void check(bool v){if(!v)std::abort();}
static int metadataCalls=0;
struct FakeString {char text[16];uint64_t len,capacity;};
static FakeString macroText{"ship_test",9,15};
static const void* FakeMetadata(void*){++metadataCalls;return &macroText;}
int main(){
    unsigned char object[128]{}; uint64_t id=19; memcpy(object+8,&id,8);
    float pos[16]{}; Registers r{};
    r.rcx=r.rdx=reinterpret_cast<uintptr_t>(object);r.r8=reinterpret_cast<uintptr_t>(pos);
    installed=sessionActive=true;SetCapture(true);SetMetadataImageRange(0,0);
    for(int i=0;i<128;++i)CaptureContact(&r);
    unsigned received=0;
    for(int frame=0;frame<4;++frame) {
        Poll();check(markerCount==32);
        for(size_t i=0;i<markerCount;++i)check(markers[(markerHead+i)%32].sequence==received+i+1);
        received+=static_cast<unsigned>(markerCount);markerCount=markerHead=markerTail=0;
    }
    check(received==128&&CaptureStats().dropped==0);
    puts("PASS: 128 burst contacts delivered in order without overwrite");
    CaptureContact(&r);Sleep(300);Poll();check(markerCount==0&&staleContacts==1);
    puts("PASS: expired contacts discarded");
    CaptureContact(&r);Stop();Contact ignored{};check(!PopContact(ignored)&&!markerCount);
    SetCapture(true);sessionActive=true;
    CaptureContact(&r);check(PopContact(ignored));
    puts("PASS: session stop clears queues; capture can restart");
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    SetMetadataImageRange(reinterpret_cast<uintptr_t>(base),nt->OptionalHeader.SizeOfImage);
    static uintptr_t vtable[5]{};vtable[4]=reinterpret_cast<uintptr_t>(&FakeMetadata);
    auto table=reinterpret_cast<uintptr_t>(vtable);memcpy(object+0x30,&table,8);
    CaptureContact(&r);Contact c{};check(PopContact(c));
    check(metadataCalls==0 && c.metadataValid==0);
    puts("PASS: unregistered metadata getter never called");
    MetadataBinding binding{table,reinterpret_cast<uintptr_t>(&FakeMetadata)};
    uintptr_t definition=reinterpret_cast<uintptr_t>(&macroText)-0x20;
    memcpy(object+0x60,&definition,8);
    SetMetadataBindings(&binding,1);CaptureContact(&r);check(PopContact(c));
    check(metadataCalls==0&&c.metadataValid==3&&!strcmp(c.contextMacro,"ship_test"));
    vtable[4]=reinterpret_cast<uintptr_t>(&main);CaptureContact(&r);check(PopContact(c)&&c.metadataValid==0);
    puts("PASS: direct metadata read without virtual calls; a replaced slot is rejected");
    SetCapture(false);
}
