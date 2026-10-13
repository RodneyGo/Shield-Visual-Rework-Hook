#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <initializer_list>
struct lua_State;
static void Check(bool ok,const char* reason) { if(!ok) { fprintf(stderr,"FAIL: %s (%lu)\n",reason,GetLastError()); std::exit(1); } }
template<class T> T Export(HMODULE m,const char* name) { auto p=reinterpret_cast<T>(GetProcAddress(m,name)); Check(p!=nullptr,name); return p; }
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
