#include "capture.h"
#include <MinHook.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
extern "C" uint64_t MissileFixture(uintptr_t,uintptr_t,const float*,const float*);
extern "C" void MissileFixtureSite();
void Check(bool v,const char* s){if(!v){fprintf(stderr,"FAIL: %s\n",s);exit(1);}}
int main(){
    unsigned char projectile[32]{},target[32]{};
    uint64_t pid=91,tid=92;memcpy(projectile+8,&pid,8);memcpy(target+8,&tid,8);
    float p[4]={2,3,4,0},m[16]={10,20,30,0, 0,0,1,0, 0,1,0,0, -1,0,0,0};
    auto call=[&](){return MissileFixture(reinterpret_cast<uintptr_t>(projectile),reinterpret_cast<uintptr_t>(target),p,m);};
    const auto expected=call();Check(expected==200,"fixture ABI baseline");
    Check(MH_Initialize()==MH_OK,"initialize");
    Check(MH_CreateHook(&MissileFixtureSite,&MissileProbeEntry,&MissileProbeTrampoline)==MH_OK,"mid-function hook");
    Check(MH_EnableHook(&MissileFixtureSite)==MH_OK,"enable");SetCapture(true);
    for(int i=0;i<100;++i){
        SetLastError(12345);Check(call()==expected,"volatile registers SIMD flags and stack preserved");Check(GetLastError()==12345,"last error preserved");
        Contact c{};Check(PopContact(c),"missile enqueued");float local[3];memcpy(local,c.spatial,12);
        Check(c.contextId==pid&&c.targetId==tid&&c.missileContact==1,"IDs and source");
        Check(local[0]==2&&local[1]==3&&local[2]==4,"target-local point must not be transformed to zone");
        Check(!c.directionSource,"no invented incoming vector");
    }
    // A large translated/rotated zone frame must never move the shield origin.
    m[0]=100000;m[1]=-80000;m[2]=5000;
    Check(call()==expected,"zone translation preserves ABI");
    Contact moved{};Check(PopContact(moved),"translated-zone contact enqueued");
    float same[3];memcpy(same,moved.spatial,12);
    Check(same[0]==2&&same[1]==3&&same[2]==4,"local capture invariant under zone transform");
    CaptureMissileContact(reinterpret_cast<uintptr_t>(projectile),reinterpret_cast<uintptr_t>(target),reinterpret_cast<float*>(1));
    Contact c{};Check(!PopContact(c),"bad pointer rejected");
    p[0]=NAN;call();Check(!PopContact(c),"NaN rejected");p[0]=2;
    SetCapture(false);call();Check(!PopContact(c),"disabled capture");
    Check(MH_DisableHook(&MissileFixtureSite)==MH_OK,"disable");Check(MH_Uninitialize()==MH_OK,"uninitialize");
    puts("PASS: missile mid-hook ABI, local coordinates independent of zone, IDs, invalid memory, finite values and disabled capture.");
}
