#include "capture.h"
#include <MinHook.h>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <cstdlib>

static void Check(bool ok, const char* message) {
    if (!ok) { fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
// Mixed integer/SIMD/stack arguments exercise the observer's forwarding ABI.
__declspec(noinline) uint64_t Target(void* a, double b, void* c, void* d, uint64_t e, double f) {
    volatile uint64_t value = reinterpret_cast<uintptr_t>(a) ^ reinterpret_cast<uintptr_t>(c) ^ reinterpret_cast<uintptr_t>(d);
    return value + static_cast<uint64_t>(b*8) + e + static_cast<uint64_t>(f*16);
}
int main() {
    // Emulate the verified producer stack: no game calls or process access.
    alignas(16) unsigned char stackFrame[2048]{};
    Registers frameRegs{};frameRegs.rsp=reinterpret_cast<uintptr_t>(stackFrame+0x100);
    uintptr_t producerFrame=reinterpret_cast<uintptr_t>(stackFrame+0x400);
    frameRegs.r8=producerFrame-0x60;
    uintptr_t image=0x140000000,ret=image+0xd50613,outer=image+0xd4fa40;
    memcpy(reinterpret_cast<void*>(frameRegs.rsp),&ret,8);
    memcpy(reinterpret_cast<void*>(frameRegs.rsp+0x70),&outer,8);
    memcpy(reinterpret_cast<void*>(frameRegs.rsp+0x90),&producerFrame,8);
    float spatial[16]={0,0,0,0, 0,0,1,0, 0,1,0,0, -1,0,0,0};
    float segment[4]={0,0,2000,0};memcpy(reinterpret_cast<void*>(producerFrame+0x70),segment,16);
    float direction[3]{};unsigned source=0;
    SetMetadataImageRange(image,0x10000000);
    Check(!ReadContactDirection(frameRegs,reinterpret_cast<unsigned char*>(spatial),direction,source),"unconfigured direction path rejected");
    ConfigureDirectionReturns(image+0xd4fa40,image+0xd4ffb1,image+0xd50613,image+0xd5065a);
    Check(ReadContactDirection(frameRegs,reinterpret_cast<unsigned char*>(spatial),direction,source)&&source==1&&direction[0]==-1&&direction[1]==0&&direction[2]==0,"producer cast transformed to target frame");
    ret=image+0xd5065a;outer=image+0xd4ffb1;
    memcpy(reinterpret_cast<void*>(frameRegs.rsp),&ret,8);memcpy(reinterpret_cast<void*>(frameRegs.rsp+0x70),&outer,8);
    Check(ReadContactDirection(frameRegs,reinterpret_cast<unsigned char*>(spatial),direction,source)&&source==2,"second verified call path");
    outer=0;memcpy(reinterpret_cast<void*>(frameRegs.rsp+0x70),&outer,8);
    Check(!ReadContactDirection(frameRegs,reinterpret_cast<unsigned char*>(spatial),direction,source)&&source==0,"unknown caller rejected");
    outer=image+0xd4ffb1;memcpy(reinterpret_cast<void*>(frameRegs.rsp+0x70),&outer,8);
    segment[2]=0;memcpy(reinterpret_cast<void*>(producerFrame+0x70),segment,16);
    Check(!ReadContactDirection(frameRegs,reinterpret_cast<unsigned char*>(spatial),direction,source),"zero cast rejected");
    segment[2]=2000;memcpy(reinterpret_cast<void*>(producerFrame+0x70),segment,16);
    spatial[6]=2;
    Check(!ReadContactDirection(frameRegs,reinterpret_cast<unsigned char*>(spatial),direction,source),"invalid rotation rejected");
    spatial[6]=1;frameRegs.r8+=16;
    Check(!ReadContactDirection(frameRegs,reinterpret_cast<unsigned char*>(spatial),direction,source),"unexpected producer frame rejected");
    ConfigureDirectionReturns(0,0,0,0);
    SetMetadataImageRange(0,0);
    unsigned char a[64], b[64], c[64];
    for (unsigned i=0;i<64;++i) { a[i]=static_cast<unsigned char>(i); b[i]=static_cast<unsigned char>(127-i); c[i]=static_cast<unsigned char>(42+i); }
    char out[16];
    Check(!SafeCopy(out,reinterpret_cast<void*>(1),sizeof(out)),"unreadable input must be rejected");
    struct SSO { char data[16]; uint64_t length, capacity; } sso={"ship_macro",10,15};
    char macro[128]={};
    Check(CopyMSVCString(&sso,macro,sizeof(macro)) && !strcmp(macro,"ship_macro"),"short macro string");
    const char* longName="bullet_ter_s_test_01_macro";
    SSO heap={}; memcpy(heap.data,&longName,sizeof(longName)); heap.length=strlen(longName); heap.capacity=31;
    Check(CopyMSVCString(&heap,macro,sizeof(macro)) && !strcmp(macro,longName),"heap macro string");
    heap.length=99999;
    Check(!CopyMSVCString(&heap,macro,sizeof(macro)),"invalid string length rejected");
    Check(MH_Initialize()==MH_OK,"MinHook init");
    using Fn = decltype(&Target);
    Fn volatile call = &Target;
    auto expected = call(a,12.5,b,c,123,5.25);
    Check(MH_CreateHook(reinterpret_cast<void*>(&Target),&ProbeEntry,&ProbeTrampoline)==MH_OK,"create hook");
    Check(MH_EnableHook(reinterpret_cast<void*>(&Target))==MH_OK,"enable hook");
    SetCapture(true);
    SetLastError(8675309);
    Check(call(a,12.5,b,c,123,5.25)==expected,"mixed arguments and return must survive hook");
    Check(GetLastError()==8675309,"last-error must survive observer");
    Contact rec;
    Check(PopContact(rec),"first sample exists");
    Check(rec.regs.rcx==reinterpret_cast<uintptr_t>(a) && rec.regs.r8==reinterpret_cast<uintptr_t>(b) && rec.regs.r9==reinterpret_cast<uintptr_t>(c),"register positions");
    Check(!memcmp(rec.context,a,32) && !memcmp(rec.spatial,b,64) && !memcmp(rec.extra,c,16),"snapshots match input");
    double simd; memcpy(&simd,rec.regs.xmm[1],8);
    Check(simd==12.5,"XMM argument capture");
    uint64_t arg5; double arg6;
    memcpy(&arg5,rec.stack,8); memcpy(&arg6,rec.stack+8,8);
    Check(arg5==123 && arg6==5.25,"stack argument capture");
    for (unsigned i=0;i<1000;++i) Check(call(a,12.5,b,c,123,5.25)==expected,"overflow forwarding");
    Check(CaptureStats().dropped>0,"full queue must drop samples");
    while(PopContact(rec)) {}
    const auto beforeContinuous=CaptureStats().captured;
    for (unsigned i=0;i<8192;++i) {
        Check(call(a,12.5,b,c,123,5.25)==expected,"continuous forwarding");
        Check(PopContact(rec),"capture continues beyond former 4096-contact limit");
    }
    Check(CaptureStats().captured==beforeContinuous+8192,"continuous session receives all drained contacts");
    std::atomic<bool> running{true}, mismatch{false};
    std::thread consumer([&] { Contact item; while(running) { while(PopContact(item)) {} std::this_thread::yield(); } });
    std::vector<std::thread> workers;
    for(int j=0;j<4;++j) workers.emplace_back([&] { for(int i=0;i<20000;++i) if(call(a,12.5,b,c,123,5.25)!=expected) mismatch=true; });
    for(auto& w:workers) w.join();
    running=false; consumer.join();
    Check(!mismatch,"concurrent forwarding");
    SetCapture(false);
    auto seen=CaptureStats().seen;
    Check(call(a,12.5,b,c,123,5.25)==expected,"disabled forwarding");
    Check(CaptureStats().seen==seen,"disabled capture does not count");
    // No worker remains inside the hook: teardown is safe in this harness only.
    Check(MH_DisableHook(reinterpret_cast<void*>(&Target))==MH_OK,"disable hook");
    Check(MH_Uninitialize()==MH_OK,"uninitialize");
    auto s=CaptureStats();
    printf("PASS: ABI, snapshots, unreadable memory, last-error, overflow, concurrency, continuous capture beyond 8192 contacts, disabled path. seen=%llu captured=%llu dropped=%llu\n",s.seen,s.captured,s.dropped);
    return 0;
}
