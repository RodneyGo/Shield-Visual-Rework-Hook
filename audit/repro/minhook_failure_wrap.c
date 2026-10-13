// Fault injection only for this standalone process; production files unchanged.
#include <Windows.h>
#include <TlHelp32.h>
int reviewFailure=0;
static HANDLE WINAPI ReviewSnapshot(DWORD flags,DWORD pid){
    if(reviewFailure==1){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return INVALID_HANDLE_VALUE;}
    return CreateToolhelp32Snapshot(flags,pid);
}
static DWORD WINAPI ReviewSuspend(HANDLE thread){
    if(reviewFailure==2){SetLastError(ERROR_ACCESS_DENIED);return (DWORD)-1;}
    return SuspendThread(thread);
}
#define CreateToolhelp32Snapshot ReviewSnapshot
#define SuspendThread ReviewSuspend
#include "hook.c"
