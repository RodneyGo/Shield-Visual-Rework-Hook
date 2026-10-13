#include <Windows.h>
#include <MinHook.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <atomic>
extern "C" int reviewFailure,reviewSuspends,reviewResumes;
extern "C" void* reviewTarget;
__declspec(noinline) int Target(int x){volatile int y=x;return y*7+1;}
using Fn=int(*)(int);Fn original=nullptr;
int Replacement(int x){return original(x);}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    std::atomic<bool> run{true};std::thread worker([&]{while(run)Sleep(1);});
    std::thread worker2([&]{while(run)Sleep(1);});
    reviewTarget=reinterpret_cast<void*>(&Target);
    if(MH_Initialize()!=MH_OK)return 3;
    if(MH_CreateHook(reinterpret_cast<void*>(&Target),reinterpret_cast<void*>(&Replacement),reinterpret_cast<void**>(&original))!=MH_OK)return 4;
    reviewFailure=std::atoi(argv[1]);
    unsigned char before[16];memcpy(before,reinterpret_cast<void*>(&Target),sizeof(before));
    auto status=MH_EnableHook(reinterpret_cast<void*>(&Target));
    Fn volatile call=&Target;
    std::printf("injected_failure=%d enable_status=%d forwarded=%d\n",reviewFailure,int(status),call(3));
    if(memcmp(before,reinterpret_cast<void*>(&Target),sizeof(before)))std::abort();
    if(reviewFailure==6&&reviewResumes!=1)std::abort();
    reviewFailure=0;run=false;worker.join();worker2.join();MH_DisableHook(reinterpret_cast<void*>(&Target));MH_Uninitialize();
    return status==MH_ERROR_THREAD_FAILURE?0:5;
}
