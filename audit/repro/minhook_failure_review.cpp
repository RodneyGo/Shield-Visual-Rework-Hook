#include <Windows.h>
#include <MinHook.h>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <atomic>
extern "C" int reviewFailure;
__declspec(noinline) int Target(int x){volatile int y=x;return y*7+1;}
using Fn=int(*)(int);Fn original=nullptr;
int Replacement(int x){return original(x);}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    std::atomic<bool> run{true};std::thread worker([&]{while(run)Sleep(1);});
    if(MH_Initialize()!=MH_OK)return 3;
    if(MH_CreateHook(reinterpret_cast<void*>(&Target),reinterpret_cast<void*>(&Replacement),reinterpret_cast<void**>(&original))!=MH_OK)return 4;
    reviewFailure=std::atoi(argv[1]);
    auto status=MH_EnableHook(reinterpret_cast<void*>(&Target));
    Fn volatile call=&Target;
    std::printf("injected_failure=%d enable_status=%d forwarded=%d\n",reviewFailure,int(status),call(3));
    reviewFailure=0;run=false;worker.join();MH_DisableHook(reinterpret_cast<void*>(&Target));MH_Uninitialize();
    return status==MH_OK?0:5;
}
