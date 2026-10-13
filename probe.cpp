#include "capture.h"
#include "compatibility.h"
#include "compatibility_profiles.h"
#include "abi.h"
#include "shell.h"
#include "fleet_shell.h"
#include <MinHook.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
#include <cmath>
#include <set>

namespace {
uintptr_t contactRva=0, missileRva=0;
bool contactCompatible=false;
HMODULE selfModule;
std::wstring extensionsDirectory;
std::set<std::string> registeredAddons;
FILE* logFile = nullptr;
bool detailedDiagnostics = false;
bool initialized = false, compatible = false, installed = false, attempted = false;
bool missileCompatible = false;
bool contactInstalled=false,missileInstalled=false,sessionActive=false,directionCompatible=false;
DWORD uiThread=0;
std::vector<MetadataBinding> metadataBindings;
ULONGLONG startTime = 0, lastDrain = 0, lastSummary = 0;
char status[512] = "SVR CONTACT: not initialized";
uintptr_t imageBase = 0;
Contact markers[32];
size_t markerHead=0, markerTail=0, markerCount=0;
unsigned noteCount=0;
constexpr unsigned DiagnosticRecordLimit=4096, DiagnosticSummaryLimit=120;
unsigned loggedRecords=0, loggedSummaries=0;
uint64_t staleContacts=0,invalidContacts=0,deliveredContacts=0;
float lastDirection[3]{};
unsigned lastDirectionSource=0;

void SetStatus(const char* value) { strcpy_s(status, value); }
void Stop() {
    ResetCapture();sessionActive=false;
    markerHead=markerTail=markerCount=0;lastDirectionSource=0;
    if(installed)SetStatus("SVR: capture paused; waiting for a loaded game");
}
void FlushLog() {
    if (!logFile) return;
    if (fflush(logFile) != 0 || ferror(logFile)) {
        fclose(logFile); logFile=nullptr;
        // Diagnostics are optional: losing the log must never stop live effects.
    }
}
std::string ExeHash(const wchar_t* path) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    unsigned char digest[32] = {};
    bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (ok) ok = BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    unsigned char buffer[65536];
    while (ok) {
        DWORD read = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &read, nullptr)) { ok = false; break; }
        if (!read) break;
        ok = BCryptHashData(hash, buffer, read, 0) >= 0;
    }
    if (ok) ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(file);
    if (!ok) return {};
    char hex[65];
    for (unsigned i = 0; i < 32; ++i) sprintf_s(hex + i * 2, 3, "%02x", digest[i]);
    return hex;
}
void WriteHex(const void* ptr, size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(ptr);
    for (size_t i = 0; i < size; ++i) fprintf(logFile, "%02x", bytes[i]);
}
void WriteRecord(const Contact& c) {
    if (!detailedDiagnostics || !logFile || loggedRecords>=DiagnosticRecordLimit) return;
    ++loggedRecords;
    fprintf(logFile, "{\"type\":\"contact\",\"seq\":%llu,\"qpc\":%llu,\"thread\":%lu,\"valid\":%lu,", c.sequence,c.ticks,c.thread,c.valid);
    fprintf(logFile, "\"rcx\":\"%llx\",\"rdx\":\"%llx\",\"r8\":\"%llx\",\"r9\":\"%llx\",", c.regs.rcx,c.regs.rdx,c.regs.r8,c.regs.r9);
    fprintf(logFile, "\"target_id\":\"%llu\",\"context_id\":\"%llu\",\"target_macro\":\"%s\",\"bullet_macro\":\"%s\",\"metadata_valid\":%u,",c.targetId,c.contextId,c.targetMacro,c.contextMacro,c.metadataValid);
    fprintf(logFile,"\"direction_source\":%u,\"incoming\":[%.9g,%.9g,%.9g],",c.directionSource,c.incoming[0],c.incoming[1],c.incoming[2]);
    fprintf(logFile,"\"missile_contact\":%u,",c.missileContact);
    fprintf(logFile, "\"context\":\""); WriteHex(c.context,sizeof(c.context));
    fprintf(logFile, "\",\"target\":\""); WriteHex(c.target,sizeof(c.target));
    fprintf(logFile, "\",\"spatial\":\""); WriteHex(c.spatial,sizeof(c.spatial));
    fprintf(logFile, "\",\"extra\":\""); WriteHex(c.extra,sizeof(c.extra));
    fprintf(logFile, "\",\"stack\":\""); WriteHex(c.stack,sizeof(c.stack));
    fprintf(logFile, "\",\"xmm\":\""); WriteHex(c.regs.xmm,sizeof(c.regs.xmm));
    fprintf(logFile, "\"}\n");
    if (loggedRecords==DiagnosticRecordLimit)
        fprintf(logFile,"{\"type\":\"diagnostic_record_limit\",\"capture_continues\":true}\n");
}
void Summary() {
    if (!logFile || loggedSummaries>=DiagnosticSummaryLimit) return;
    ++loggedSummaries;
    const auto s = CaptureStats();
    const auto cache=FleetCacheStatistics();
    fprintf(logFile,"{\"type\":\"geometry_cache\",\"load_attempts\":%llu,\"failures\":%llu,\"bytes\":%llu,\"last_load_us\":%llu}\n",cache.attempts,cache.failures,cache.bytes,cache.lastLoadMicroseconds);
    fprintf(logFile,"{\"type\":\"delivery\",\"delivered\":%llu,\"expired\":%llu,\"invalid\":%llu,\"capture_dropped\":%llu}\n",deliveredContacts,staleContacts,invalidContacts,s.dropped);
    fprintf(logFile, "{\"type\":\"summary\",\"elapsed_ms\":%llu,\"seen\":%llu,\"captured\":%llu,\"dropped\":%llu,\"read_faults\":%llu,\"limited\":%llu,\"stopped\":%s}\n",
        GetTickCount64()-startTime,s.seen,s.captured,s.dropped,s.faults,s.limited,"false");
    FlushLog();
}
void Initialize() {
    if (initialized) return;
    initialized = true;
    wchar_t exe[MAX_PATH], dll[MAX_PATH];
    const DWORD exeLength=GetModuleFileNameW(nullptr,exe,MAX_PATH),dllLength=GetModuleFileNameW(selfModule,dll,MAX_PATH);
    if (!exeLength||exeLength>=MAX_PATH||!dllLength||dllLength>=MAX_PATH) {
        SetStatus("SVR CONTACT: cannot resolve paths; disabled"); return;
    }
    // Keep observer and trampoline alive through UI reloads until process exit.
    HMODULE pinned;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&ProbeEntry), &pinned)) {
        SetStatus("SVR CONTACT: module pin failed; disabled"); return;
    }
    wchar_t* sep = wcsrchr(dll,L'\\');
    if (!sep) return;
    *sep = 0; // native directory
    sep = wcsrchr(dll,L'\\');
    if (!sep) return;
    *sep = 0; // extension directory
    extensionsDirectory=std::wstring(dll);
    extensionsDirectory.resize(extensionsDirectory.find_last_of(L"\\/"));
    const auto diagnosticPath = std::wstring(dll) + L"\\diagnostics.enabled";
    const DWORD diagnosticAttributes = GetFileAttributesW(diagnosticPath.c_str());
    detailedDiagnostics = diagnosticAttributes != INVALID_FILE_ATTRIBUTES && !(diagnosticAttributes & FILE_ATTRIBUTE_DIRECTORY);
    std::wstring directory = std::wstring(dll) + L"\\logs";
    CreateDirectoryW(directory.c_str(),nullptr);
    SYSTEMTIME t; GetLocalTime(&t);
    wchar_t name[160];
    swprintf_s(name,L"\\contact-%04u%02u%02u-%02u%02u%02u-%lu.jsonl",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,GetCurrentProcessId());
    if (_wfopen_s(&logFile,(directory+name).c_str(),L"wb")) logFile=nullptr;
    const auto hash = ExeHash(exe);
    imageBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    LARGE_INTEGER frequency; QueryPerformanceFrequency(&frequency);
    if (logFile) fprintf(logFile,"{\"type\":\"startup\",\"version\":\"0.54-dev2\",\"exe_sha256\":\"%s\",\"detailed_diagnostics\":%s}\n",hash.c_str(),detailedDiagnostics?"true":"false");
    const auto* dosHeader=reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
    const auto* ntHeader=reinterpret_cast<const IMAGE_NT_HEADERS64*>(imageBase+dosHeader->e_lfanew);
    std::span<const unsigned char> image(reinterpret_cast<const unsigned char*>(imageBase),ntHeader->OptionalHeader.SizeOfImage);
    const bool abi=VerifyABI(exe,image,imageBase,metadataBindings);
    if(logFile)fprintf(logFile,"{\"type\":\"abi_boundary\",\"accepted\":%s,\"metadata_tables\":%zu}\n",abi?"true":"false",metadataBindings.size());
    if(!abi){SetStatus("SVR: unreviewed code/data ABI; native capture disabled");FlushLog();return;}
    FunctionMatch matches[5];
    for(unsigned i=0;i<5;++i){
        matches[i]=FindFunction(image,Profiles[i]);
        if(logFile)fprintf(logFile,"{\"type\":\"function_profile\",\"name\":\"%s\",\"matches\":%u,\"rva\":%u}\n",Profiles[i].name,matches[i].matches,matches[i].hook);
    }
    contactCompatible=matches[0].unique();
    missileCompatible=matches[1].unique() && matches[3].unique();
    contactRva=matches[0].hook;missileRva=matches[1].hook;
    directionCompatible=matches[2].unique() && matches[4].unique();
    ConfigureDirectionReturns(directionCompatible?imageBase+matches[2].entry+0x800:0,
                              directionCompatible?imageBase+matches[2].entry+0xd71:0,
                              directionCompatible?imageBase+matches[4].entry+0x113:0,
                              directionCompatible?imageBase+matches[4].entry+0x15a:0);
    if (contactCompatible || missileCompatible) {
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
        const auto* pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(imageBase+dos->e_lfanew);
        SetMetadataImageRange(imageBase,pe->OptionalHeader.SizeOfImage);
        SetMetadataBindings(metadataBindings.data(),metadataBindings.size());
        const bool fleet=ConfigureFleetShells(std::wstring(dll)+L"\\native\\fleet_shells.bin");
        if(logFile) fprintf(logFile,"{\"type\":\"fleet_geometry\",\"ready\":%s,\"profiles\":%u}\n",fleet?"true":"false",FleetProfileCount());
        compatible=fleet;
    }
    SetStatus(compatible ? "SVR CONTACT: compatible; waiting for a loaded game" :
        (contactCompatible||missileCompatible)?"SVR: fleet geometry unavailable; native capture disabled":"SVR CONTACT: no unique complete function profiles; disabled");
    if (logFile) fprintf(logFile,"{\"type\":\"compatibility\",\"accepted\":%s}\n",compatible?"true":"false");
    FlushLog();
}
void Start() {
    if (!compatible || sessionActive) return;
    if(attempted) {
        if(installed){Stop();SetCapture(true);sessionActive=true;}
    } else {
    attempted = true;
    const auto init = MH_Initialize();
    if (init != MH_OK) { SetStatus("SVR CONTACT: MinHook initialization failed; disabled"); return; }
    auto install=[&](uintptr_t rva,void* detour,void** trampoline,const char* name){
        void* target=reinterpret_cast<void*>(imageBase+rva);
        auto result=MH_CreateHook(target,detour,trampoline);
        if(result==MH_OK)result=MH_EnableHook(target);
        if(logFile)fprintf(logFile,"{\"type\":\"hook_result\",\"name\":\"%s\",\"installed\":%s,\"status\":%d}\n",name,result==MH_OK?"true":"false",int(result));
        return result==MH_OK;
    };
    if(contactCompatible)contactInstalled=install(contactRva,reinterpret_cast<void*>(&ProbeEntry),&ProbeTrampoline,"contact");
    if(missileCompatible)missileInstalled=install(missileRva,reinterpret_cast<void*>(&MissileProbeEntry),&MissileProbeTrampoline,"missile");
    installed=contactInstalled||missileInstalled;
    if(!installed){SetStatus("SVR: compatible profiles found but hooks failed; disabled");FlushLog();return;}
    startTime = lastDrain = lastSummary = GetTickCount64();
    Stop();SetCapture(true);sessionActive=true;
    if (logFile) fprintf(logFile,"{\"type\":\"armed\",\"duration_ms\":null,\"capture_limit\":null}\n");
    FlushLog();
    }
    if(sessionActive)sprintf_s(status,"SVR: ACTIVE; contact=%s missile=%s direction=%s profiles=%u",contactInstalled?"on":"OFF",missileInstalled?"on":"OFF",directionCompatible?"on":"OFF",FleetProfileCount());
}
void Poll() {
    if (!sessionActive) return;
    const auto now = GetTickCount64();
    {
        lastDrain = now;
        Contact c;
        for (unsigned i = 0; i < 128 && markerCount<std::size(markers) && PopContact(c); ++i) {
            if(!ContactFresh(c)){++staleContacts;continue;}
            WriteRecord(c);
            float position[3]; memcpy(position,c.spatial,sizeof(position));
            bool valid=(c.valid & 6)==6 && c.targetId;
            for(float coordinate:position) valid=valid && std::isfinite(coordinate) && std::abs(coordinate)<1000000;
            // Latest bounded samples only; UI resolves target/root by ID, never raw pointer.
            if(valid) {
                markers[markerTail]=c;markerTail=(markerTail+1)%32;++markerCount;
            } else ++invalidContacts;
        }
        FlushLog();
    }
    if (now-lastSummary >= 5000 && loggedSummaries<DiagnosticSummaryLimit) { lastSummary = now; Summary(); }
}
// Resolve only the small Lua API used here from the game's existing LuaJIT DLL.
struct lua_State;
using LuaFn = int(*)(lua_State*);
void (*createTable)(lua_State*,int,int);
void (*pushClosure)(lua_State*,LuaFn,int);
void (*setField)(lua_State*,int,const char*);
void (*pushString)(lua_State*,const char*);
const char* (*toString)(lua_State*,int,size_t*);
void (*pushNil)(lua_State*);
int LuaStart(lua_State* L) { Start(); pushString(L,status); return 1; }
int LuaStop(lua_State* L) { Stop();pushString(L,status);return 1; }
int LuaPoll(lua_State* L) { Poll(); pushString(L,status); return 1; }
int LuaStatus(lua_State* L) { pushString(L,status); return 1; }
int LuaMarker(lua_State* L) {
    lastDirectionSource=0;
    while(markerCount&&!ContactFresh(markers[markerHead])){markerHead=(markerHead+1)%32;--markerCount;++staleContacts;}
    if(!markerCount) { pushNil(L); return 1; }
    const auto& c=markers[markerHead];
    memcpy(lastDirection,c.incoming,sizeof(lastDirection));lastDirectionSource=c.directionSource;
    float p[3]; memcpy(p,c.spatial,sizeof(p));
    char line[512];
    sprintf_s(line,"%llu|%llu|%.9g|%.9g|%.9g|%s|%s",c.sequence,c.targetId,p[0],p[1],p[2],c.contextMacro,c.targetMacro);
    markerHead=(markerHead+1)%32;--markerCount;
    ++deliveredContacts;
    pushString(L,line);return 1;
}
int LuaMarkerDirection(lua_State* L) {
    if(!lastDirectionSource){pushNil(L);return 1;}
    char line[120];sprintf_s(line,"%.9g|%.9g|%.9g|%u",lastDirection[0],lastDirection[1],lastDirection[2],lastDirectionSource);
    pushString(L,line);return 1;
}
int LuaNote(lua_State* L) {
    if(!logFile || noteCount>=256) return 0;
    size_t length=0;const char* text=toString(L,1,&length);
    if(!text) return 0;
    ++noteCount;fprintf(logFile,"{\"type\":\"bridge\",\"note\":\"");
    for(size_t i=0;i<length && i<1024;++i) {
        const unsigned char c=text[i];
        if(c=='"' || c=='\\') fputc('\\',logFile);
        if(c>=32) fputc(c,logFile);
    }
    fprintf(logFile,"\"}\n");FlushLog();return 0;
}
int LuaFleetProject(lua_State* L) {
    size_t length=0;const char* input=toString(L,1,&length);
    unsigned profile=0;Vec3 point{};ShellProjection projected{};char trailing=0;
    if(!compatible||!input||length>140||sscanf_s(input,"%u|%f|%f|%f%c",&profile,&point.x,&point.y,&point.z,&trailing,1)!=4||!ProjectFleet(profile,point,projected)) {
        pushNil(L);return 1;
    }
    char line[200];sprintf_s(line,"%u|%.9g|%.9g|%.9g|%.9g|%u",projected.code,projected.point.x,projected.point.y,projected.point.z,projected.distance,projected.tested);
    pushString(L,line);return 1;
}
int LuaFleetProjectIncoming(lua_State* L) {
    size_t length=0;const char* input=toString(L,1,&length);
    unsigned profile=0;Vec3 point{},direction{};ShellProjection projected{};char trailing=0;
    if(!compatible||!input||length>240||sscanf_s(input,"%u|%f|%f|%f|%f|%f|%f%c",&profile,&point.x,&point.y,&point.z,&direction.x,&direction.y,&direction.z,&trailing,1)!=7||!ProjectFleetIncoming(profile,point,direction,projected)) {
        pushNil(L);return 1;
    }
    char line[200];sprintf_s(line,"%u|%.9g|%.9g|%.9g|%.9g|%u",projected.code,projected.point.x,projected.point.y,projected.point.z,projected.distance,projected.tested);
    pushString(L,line);return 1;
}
int LuaRegisterAddon(lua_State* L) {
    size_t length=0;const char* input=toString(L,1,&length);
    if(!compatible||!input||!length||length>80||extensionsDirectory.empty()){pushNil(L);return 1;}
    for(size_t i=0;i<length;++i) if(!((input[i]>='a'&&input[i]<='z')||(input[i]>='0'&&input[i]<='9')||input[i]=='_')){pushNil(L);return 1;}
    bool ready=false;
    try {
      const std::string id(input,length);
      if(registeredAddons.count(id))ready=true;
      else {
        const std::wstring folder(id.begin(),id.end());
        // Allocate the registry node before publishing geometry; undo on failure.
        auto inserted=registeredAddons.insert(id).first;
        try {ready=AddFleetShells(extensionsDirectory+L"\\"+folder+L"\\native\\fleet_shells.bin");}
        catch(...){registeredAddons.erase(inserted);throw;}
        if(!ready)registeredAddons.erase(inserted);
        if(ready&&logFile){fprintf(logFile,"{\"type\":\"addon_registered\",\"id\":\"%s\",\"total_profiles\":%u}\n",id.c_str(),FleetProfileCount());FlushLog();}
      }
    }
    catch(...){ready=false;}
    if(ready)pushString(L,"ready");else pushNil(L);return 1;
}
int LuaHasProfile(lua_State* L) {
    size_t length=0;const char* input=toString(L,1,&length);unsigned id=0;char trailing=0;
    if(!compatible||!input||length>10||sscanf_s(input,"%u%c",&id,&trailing,1)!=1||!HasFleetProfile(id)){pushNil(L);return 1;}
    pushString(L,"ready");return 1;
}
template<LuaFn F> int OnUIThread(lua_State* L) {
    if(GetCurrentThreadId()!=uiThread){pushNil(L);return 1;}
    return F(L);
}
}
extern "C" __declspec(dllexport) int luaopen_svr_contact_probe(lua_State* L) {
    if(uiThread&&uiThread!=GetCurrentThreadId())return 0;
    uiThread=GetCurrentThreadId();
    Stop(); // A new UI loader must not replay contacts retained by the old UI.
    HMODULE lua = GetModuleHandleW(L"lua51_64.dll");
    if (!lua) return 0;
    createTable = reinterpret_cast<decltype(createTable)>(GetProcAddress(lua,"lua_createtable"));
    pushClosure = reinterpret_cast<decltype(pushClosure)>(GetProcAddress(lua,"lua_pushcclosure"));
    setField = reinterpret_cast<decltype(setField)>(GetProcAddress(lua,"lua_setfield"));
    pushString = reinterpret_cast<decltype(pushString)>(GetProcAddress(lua,"lua_pushstring"));
    toString = reinterpret_cast<decltype(toString)>(GetProcAddress(lua,"lua_tolstring"));
    pushNil = reinterpret_cast<decltype(pushNil)>(GetProcAddress(lua,"lua_pushnil"));
    if (!createTable || !pushClosure || !setField || !pushString || !toString || !pushNil) return 0;
    try { Initialize(); }
    catch (...) {
        compatible=contactCompatible=missileCompatible=directionCompatible=false;
        SetMetadataBindings(nullptr,0);ConfigureDirectionReturns(0,0,0,0);Stop();
        SetStatus("SVR CONTACT: initialization failed; disabled");
    }
    createTable(L,0,12);
    pushClosure(L,&OnUIThread<LuaStart>,0); setField(L,-2,"start");
    pushClosure(L,&OnUIThread<LuaStop>,0); setField(L,-2,"stop");
    pushClosure(L,&OnUIThread<LuaPoll>,0); setField(L,-2,"poll");
    pushClosure(L,&OnUIThread<LuaStatus>,0); setField(L,-2,"status");
    pushClosure(L,&OnUIThread<LuaMarker>,0); setField(L,-2,"marker");
    pushClosure(L,&OnUIThread<LuaNote>,0); setField(L,-2,"note");
    pushClosure(L,&OnUIThread<LuaFleetProject>,0); setField(L,-2,"fleet_project");
    pushClosure(L,&OnUIThread<LuaMarkerDirection>,0); setField(L,-2,"marker_direction");
    pushClosure(L,&OnUIThread<LuaFleetProjectIncoming>,0); setField(L,-2,"fleet_project_incoming");
    pushClosure(L,&OnUIThread<LuaRegisterAddon>,0); setField(L,-2,"register_addon");
    pushClosure(L,&OnUIThread<LuaHasProfile>,0); setField(L,-2,"has_profile");
    return 1;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) selfModule = module;
    // No file I/O, hook removal, or waiting while holding the loader lock.
    return TRUE;
}
