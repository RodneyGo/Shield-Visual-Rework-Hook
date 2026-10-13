#pragma once
#include <Windows.h>

struct lua_State;
using LuaFn = int(*)(lua_State*);

// A Lua VM can move between OS threads. Protect native state, not thread identity.
struct LuaDispatch {
    SRWLOCK lock = SRWLOCK_INIT;
    int (*getTop)(lua_State*) = nullptr;
    void (*pushClosure)(lua_State*,LuaFn,int) = nullptr;
    void (*insert)(lua_State*,int) = nullptr;
    int (*pcall)(lua_State*,int,int,int) = nullptr;
    int (*error)(lua_State*) = nullptr;
    void (*pushString)(lua_State*,const char*) = nullptr;

    template<LuaFn F> int Call(lua_State* L) {
        const int arguments = getTop(L);
        // Allocate the call frame before taking the lock: Lua allocation can raise.
        pushClosure(L,F,0);
        insert(L,1);
        if (!TryAcquireSRWLockExclusive(&lock)) {
            pushString(L,"SVR: concurrent or reentrant native API call");
            return error(L);
        }
        // Lua errors (including allocation failure) must release the native lock.
        const int result = pcall(L,arguments,-1,0);
        ReleaseSRWLockExclusive(&lock);
        if (result) return error(L);
        return getTop(L);
    }
};
