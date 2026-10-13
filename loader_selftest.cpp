#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <initializer_list>
#include <thread>
#include "lua_dispatch.h"
struct lua_State;
static void Check(bool ok,const char* reason) { if(!ok) { fprintf(stderr,"FAIL: %s (%lu)\n",reason,GetLastError()); std::exit(1); } }
template<class T> T Export(HMODULE m,const char* name) { auto p=reinterpret_cast<T>(GetProcAddress(m,name)); Check(p!=nullptr,name); return p; }
static LuaDispatch testDispatch;
static int Echo(lua_State* L){return testDispatch.getTop(L);}
static int Raise(lua_State* L){testDispatch.pushString(L,"expected test error");return testDispatch.error(L);}
static int EchoCall(lua_State* L){return testDispatch.Call<Echo>(L);}
static int RaiseCall(lua_State* L){return testDispatch.Call<Raise>(L);}
int wmain(int argc,wchar_t** argv) {
    Check(argc==5,"expected Lua DLL, probe DLL, Lua bootstrap and bridge-test paths");
    auto lua=LoadLibraryW(argv[1]); Check(lua!=nullptr,"load game LuaJIT in isolated test host");
    auto create=Export<lua_State*(*)()>(lua,"luaL_newstate");
    auto close=Export<void(*)(lua_State*)>(lua,"lua_close");
    auto loadfile=Export<int(*)(lua_State*,const char*)>(lua,"luaL_loadfile");
    auto getfield=Export<void(*)(lua_State*,int,const char*)>(lua,"lua_getfield");
    auto pcall=Export<int(*)(lua_State*,int,int,int)>(lua,"lua_pcall");
    auto tostring=Export<const char*(*)(lua_State*,int,size_t*)>(lua,"lua_tolstring");
    auto type=Export<int(*)(lua_State*,int)>(lua,"lua_type");
    auto settop=Export<void(*)(lua_State*,int)>(lua,"lua_settop");
    auto openlibs=Export<void(*)(lua_State*)>(lua,"luaL_openlibs");
    auto pushstring=Export<void(*)(lua_State*,const char*)>(lua,"lua_pushstring");
    auto setfield=Export<void(*)(lua_State*,int,const char*)>(lua,"lua_setfield");
    testDispatch.getTop=Export<decltype(testDispatch.getTop)>(lua,"lua_gettop");
    testDispatch.pushClosure=Export<decltype(testDispatch.pushClosure)>(lua,"lua_pushcclosure");
    testDispatch.insert=Export<decltype(testDispatch.insert)>(lua,"lua_insert");
    testDispatch.pcall=pcall;testDispatch.pushString=pushstring;
    testDispatch.error=Export<decltype(testDispatch.error)>(lua,"lua_error");
    lua_State* L=create(); Check(L!=nullptr,"Lua state");
    char script[2048]; WideCharToMultiByte(CP_UTF8,0,argv[3],-1,script,sizeof(script),nullptr,nullptr);
    Check(loadfile(L,script)==0,"Lua bootstrap syntax"); settop(L,0);
    auto dll=LoadLibraryW(argv[2]); Check(dll!=nullptr,"load probe");
    auto open=Export<int(*)(lua_State*)>(dll,"luaopen_svr_contact_probe");
    Check(open(L)==1 && type(L,-1)==5,"luaopen returns API table");
    for(const char* name : {"status","start","poll","stop"}) {
        getfield(L,-1,name); Check(pcall(L,0,1,0)==0,"Lua API call");
        const char* message=tostring(L,-1,nullptr);
        Check(message && strstr(message,"native capture disabled"),"foreign executable must fail closed");
        settop(L,-2);
    }
    getfield(L,-1,"marker");Check(pcall(L,0,1,0)==0 && type(L,-1)==0,"empty marker queue returns nil");settop(L,-2);
    // X4 may resume the same Lua VM on a different worker. Access stays sequential.
    std::thread handoff([&] {
        for(const char* name : {"status","start","poll","stop"}) {
            getfield(L,-1,name); Check(pcall(L,0,1,0)==0,"worker Lua API call");
            const char* message=tostring(L,-1,nullptr);
            Check(message && strstr(message,"native capture disabled"),"sequential worker handoff must retain native API, not return nil");
            settop(L,-2);
        }
        settop(L,0);
        Check(open(L)==1 && type(L,-1)==5,"UI reload on another worker returns API table");
    });
    handoff.join();
    puts("PASS: sequential Lua VM handoff and loader reload across OS threads");
    settop(L,0);
    testDispatch.pushClosure(L,RaiseCall,0);
    Check(pcall(L,0,0,0)!=0 && strstr(tostring(L,-1,nullptr),"expected test error"),"protected dispatch preserves Lua error");
    settop(L,0);
    // The preceding Lua error must not leave the native lock held.
    std::thread afterError([&]{
        testDispatch.pushClosure(L,EchoCall,0);pushstring(L,"first");pushstring(L,"second");
        Check(pcall(L,2,2,0)==0,"dispatch remains available after Lua error");
        Check(!strcmp(tostring(L,-2,nullptr),"first")&&!strcmp(tostring(L,-1,nullptr),"second"),"dispatch preserves arguments and return values");
        settop(L,0);
    });
    afterError.join();
    AcquireSRWLockExclusive(&testDispatch.lock);
    testDispatch.pushClosure(L,EchoCall,0);
    const int busy=pcall(L,0,0,0);
    ReleaseSRWLockExclusive(&testDispatch.lock);
    Check(busy!=0 && strstr(tostring(L,-1,nullptr),"concurrent or reentrant"),"busy dispatch reports an error without deadlocking");
    puts("PASS: protected dispatch preserves arguments/results, releases on Lua error, rejects concurrent/reentrant entry");
    settop(L,0);openlibs(L);
    pushstring(L,script);setfield(L,-10002,"PROBE_BOOTSTRAP");
    char bridge[2048]; WideCharToMultiByte(CP_UTF8,0,argv[4],-1,bridge,sizeof(bridge),nullptr,nullptr);
    Check(loadfile(L,bridge)==0,"bridge test syntax");
    const int bridgeResult=pcall(L,0,0,0);
    if(bridgeResult) fprintf(stderr,"Lua bridge: %s\n",tostring(L,-1,nullptr));
    Check(bridgeResult==0,"UI bridge filtering, throttling, local attachment, acknowledgments");
    close(L);
    puts("PASS: real LuaJIT syntax/load/API, wrong-EXE rejection, simulated UI bridge. No game process started.");
    return 0;
}
