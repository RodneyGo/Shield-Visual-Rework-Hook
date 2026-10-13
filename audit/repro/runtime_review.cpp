// Tests the unchanged candidate in this process. No game hooks are installed.
#include "probe.cpp"
#include <cassert>
#include <cstdlib>
void check(bool v){if(!v)std::abort();}
static int metadataCalls=0;
struct FakeString {char text[16];uint64_t len,capacity;};
static FakeString macroText{"ship_test",9,15};
static const void* FakeMetadata(void*){++metadataCalls;return &macroText;}
int main(){
    unsigned char object[64]{}; uint64_t id=19; memcpy(object+8,&id,8);
    float pos[16]{}; Registers r{};
    r.rcx=r.rdx=reinterpret_cast<uintptr_t>(object);r.r8=reinterpret_cast<uintptr_t>(pos);
    installed=true;SetCapture(true);SetMetadataImageRange(0,0);
    for(int i=0;i<128;++i)CaptureContact(&r);
    lastDrain=GetTickCount64()-100;Poll();
    check(CaptureStats().captured==128 && CaptureStats().dropped==0 && markerCount==32);
    std::printf("burst: captured=128, capture_dropped=0, markers=%zu, first_sequence=%llu\n",markerCount,markers[markerHead].sequence);
    check(markers[markerHead].sequence==97);
    markerCount=markerHead=markerTail=0;
    CaptureContact(&r);
    Sleep(650);
    lastDrain=GetTickCount64()-100;Poll();
    check(markerCount==1);
    LARGE_INTEGER now,freq;QueryPerformanceCounter(&now);QueryPerformanceFrequency(&freq);
    std::printf("stale: delivered_age_ms=%.1f\n",1000.*(now.QuadPart-markers[markerHead].ticks)/freq.QuadPart);
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    SetMetadataImageRange(reinterpret_cast<uintptr_t>(base),nt->OptionalHeader.SizeOfImage);
    static uintptr_t vtable[5]{};vtable[4]=reinterpret_cast<uintptr_t>(&FakeMetadata);
    auto table=reinterpret_cast<uintptr_t>(vtable);memcpy(object+0x30,&table,8);
    CaptureContact(&r);Contact c{};check(PopContact(c));
    check(metadataCalls==2 && c.metadataValid==3);
    std::printf("metadata: unprofiled_virtual_function_called=%d\n",metadataCalls);
    SetCapture(false);
}
