// Fault injection only for this standalone process; production files unchanged.
#include <Windows.h>
#include <TlHelp32.h>
int reviewFailure=0;
void* reviewTarget=NULL;
int reviewSuspends=0,reviewResumes=0;
static HANDLE WINAPI ReviewSnapshot(DWORD flags,DWORD pid){
    if(reviewFailure==1){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return INVALID_HANDLE_VALUE;}
    return CreateToolhelp32Snapshot(flags,pid);
}
static DWORD WINAPI ReviewSuspend(HANDLE thread){
    ++reviewSuspends;
    if(reviewFailure==2||(reviewFailure==6&&reviewSuspends==2)){SetLastError(ERROR_ACCESS_DENIED);return (DWORD)-1;}
    return SuspendThread(thread);
}
static HANDLE WINAPI ReviewOpen(DWORD access,BOOL inherit,DWORD id){
    if(reviewFailure==3){SetLastError(ERROR_ACCESS_DENIED);return NULL;}
    return OpenThread(access,inherit,id);
}
static BOOL WINAPI ReviewContext(HANDLE h,LPCONTEXT c){
    if(reviewFailure==4){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    BOOL result=GetThreadContext(h,c);
    if(result&&reviewFailure==5)c->Rip=(DWORD64)reviewTarget;
    return result;
}
static BOOL WINAPI ReviewSetContext(HANDLE h,const CONTEXT* c){
    if(reviewFailure==5){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    return SetThreadContext(h,c);
}
static DWORD WINAPI ReviewResume(HANDLE h){++reviewResumes;return ResumeThread(h);}
#define SetThreadContext ReviewSetContext
#define ResumeThread ReviewResume
#define OpenThread ReviewOpen
#define GetThreadContext ReviewContext
#define CreateToolhelp32Snapshot ReviewSnapshot
#define SuspendThread ReviewSuspend
#include "hook.c"
