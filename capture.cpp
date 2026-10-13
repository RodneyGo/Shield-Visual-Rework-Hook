#include "capture.h"
#include <cstring>
#include <cmath>

extern "C" { void* ProbeTrampoline = nullptr; void* MissileProbeTrampoline = nullptr; }
namespace {
SRWLOCK queueLock = SRWLOCK_INIT;
Contact queue[512];
size_t head = 0, tail = 0, count = 0;
std::atomic<bool> active{false};
std::atomic<uint64_t> seen{0}, captured{0}, dropped{0}, faults{0}, limited{0};
uintptr_t metadataBase = 0, metadataEnd = 0;
uintptr_t directionReturnA=0, directionReturnB=0;
uintptr_t innerReturnA=0, innerReturnB=0;
}
void ConfigureDirectionReturns(uintptr_t first,uintptr_t second,uintptr_t innerFirst,uintptr_t innerSecond) noexcept {directionReturnA=first;directionReturnB=second;innerReturnA=innerFirst;innerReturnB=innerSecond;}
void SetMetadataImageRange(uintptr_t base, size_t size) noexcept { metadataBase=base; metadataEnd=base+size; }
bool CopyMSVCString(const void* source, char* out, size_t capacity) noexcept {
    unsigned char data[32] = {};
    if (!source || !capacity || !SafeCopy(data,source,sizeof(data))) return false;
    uint64_t length, storage;
    memcpy(&length,data+16,8); memcpy(&storage,data+24,8);
    if (!length || length >= capacity || storage < length || storage > 1048576) return false;
    const void* text=source;
    if (storage >= 16) memcpy(&text,data,8);
    if (!text || !SafeCopy(out,text,static_cast<size_t>(length))) return false;
    out[length]=0;
    for(size_t i=0;i<length;++i) {
        const char c=out[i];
        if (!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_')) {
            out[0]=0; return false;
        }
    }
    return true;
}
static bool ReadMacro(uintptr_t object, char* out, size_t capacity) noexcept {
    if (!metadataBase || !object) return false;
    // Verified definition-interface layout, +0x30 embedded interface, slot 4.
    // Invoked only while the original handler owns valid object arguments.
    __try {
        const auto vtable=*reinterpret_cast<uintptr_t*>(object+0x30);
        if(vtable<metadataBase || vtable+40>metadataEnd) return false;
        const auto function=*reinterpret_cast<uintptr_t*>(vtable+0x20);
        if(function<metadataBase || function>=metadataEnd) return false;
        const void* text=reinterpret_cast<const void*(*)(void*)>(function)(reinterpret_cast<void*>(object+0x30));
        return CopyMSVCString(text,out,capacity);
    } __except(EXCEPTION_EXECUTE_HANDLER) { out[0]=0; return false; }
}
bool SafeCopy(void* dst, const void* src, size_t length) noexcept {
    // No delayed dereference: game-owned storage is read only during the callback.
    __try { std::memcpy(dst, src, length); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { std::memset(dst, 0, length); return false; }
}
void SetCapture(bool enabled) noexcept { active.store(enabled, std::memory_order_release); }
bool ReadContactDirection(const Registers& regs, const unsigned char* spatial, float* incoming, unsigned& source) noexcept {
    source=0; incoming[0]=incoming[1]=incoming[2]=0;
    if(!metadataBase || !regs.rsp || !regs.r8) return false;
    uintptr_t ret=0, outerRet=0, frame=0;
    if(!SafeCopy(&ret,reinterpret_cast<void*>(regs.rsp),8) ||
       !innerReturnA || !innerReturnB || (ret!=innerReturnA && ret!=innerReturnB)) return false;
    // d50500: three pushes + 0x50 locals + the call return = 0x70.
    // Its saved caller RBP lives at entry RSP+0x20, hence observer RSP+0x90.
    if(!SafeCopy(&outerRet,reinterpret_cast<void*>(regs.rsp+0x70),8) ||
       (!directionReturnA || !directionReturnB || (outerRet!=directionReturnA && outerRet!=directionReturnB)) ||
       !SafeCopy(&frame,reinterpret_cast<void*>(regs.rsp+0x90),8) || frame!=regs.r8+0x60) return false;
    // d4f240 supplies r8 = RBP-0x60. RBP+0x70 is the exact collision
    // segment delta passed to the cast at d4fc61, constructed at d4f603.
    // Restrict access to this live producer frame; do not retain game pointers.
    if(frame<regs.rsp || frame-regs.rsp>0x1000) return false;
    float segment[4], basis[16];
    if(!SafeCopy(segment,reinterpret_cast<void*>(frame+0x70),sizeof(segment))) return false;
    memcpy(basis,spatial,sizeof(basis));
    double length=0;
    for(int i=0;i<3;++i) { if(!std::isfinite(segment[i])) return false; length+=double(segment[i])*segment[i]; }
    if(length<1e-12 || length>1e14) return false;
    // Spatial basis is the inverse target rotation applied to world identity
    // in d4f931..d4fa0b / d4fe6d..d4ff27. Verify it before transforming.
    for(int i=0;i<3;++i) for(int j=0;j<3;++j) {
        double dot=0;
        for(int k=0;k<3;++k) {
            float a=basis[4+4*i+k],b=basis[4+4*j+k];
            if(!std::isfinite(a)||!std::isfinite(b)) return false;
            dot+=double(a)*b;
        }
        if(std::abs(dot-(i==j?1.0:0.0))>0.01) return false;
    }
    const double inv=1.0/std::sqrt(length);
    for(int k=0;k<3;++k) incoming[k]=float((double(basis[4+k])*segment[0]+double(basis[8+k])*segment[1]+double(basis[12+k])*segment[2])*inv);
    source=outerRet==directionReturnA?1:2;
    return true;
}
Stats CaptureStats() noexcept { return {seen.load(), captured.load(), dropped.load(), faults.load(), limited.load()}; }
static void CaptureStoredContact(const Registers* regs, bool missile) noexcept {
    if (!active.load(std::memory_order_acquire)) return;
    const auto sequence = seen.fetch_add(1, std::memory_order_relaxed) + 1;
    const DWORD savedError = GetLastError();
    // Contention drops a visual contact instead of blocking the game thread.
    if (!TryAcquireSRWLockExclusive(&queueLock)) { ++dropped; SetLastError(savedError); return; }
    if (count == 512) {
        ++dropped; ReleaseSRWLockExclusive(&queueLock); SetLastError(savedError); return;
    }
    auto& rec = queue[tail];
    rec = {};
    rec.sequence = sequence;
    LARGE_INTEGER ticks; QueryPerformanceCounter(&ticks);
    rec.ticks = ticks.QuadPart;
    rec.thread = GetCurrentThreadId();
    rec.regs = *regs;
    rec.missileContact = missile ? 1 : 0;
    const void* sources[] = {reinterpret_cast<void*>(regs->rcx), reinterpret_cast<void*>(regs->rdx),
        reinterpret_cast<void*>(regs->r8), reinterpret_cast<void*>(regs->r9), reinterpret_cast<void*>(regs->rsp + 0x28)};
    void* dests[] = {rec.context, rec.target, rec.spatial, rec.extra, rec.stack};
    const size_t lengths[] = {32, 32, 64, 16, 64};
    for (unsigned i = 0; i < 5; ++i) {
        if(missile && i>=3) continue;
        if (sources[i] && SafeCopy(dests[i], sources[i], lengths[i])) rec.valid |= 1u << i;
        else ++faults;
    }
    memcpy(&rec.contextId,rec.context+8,8);
    memcpy(&rec.targetId,rec.target+8,8);
    if(ReadMacro(regs->rcx,rec.contextMacro,sizeof(rec.contextMacro))) rec.metadataValid|=1;
    if(ReadMacro(regs->rdx,rec.targetMacro,sizeof(rec.targetMacro))) rec.metadataValid|=2;
    if(!missile && (rec.valid & 4)) ReadContactDirection(*regs,rec.spatial,rec.incoming,rec.directionSource);
    tail = (tail + 1) % 512; ++count; ++captured;
    ReleaseSRWLockExclusive(&queueLock);
    SetLastError(savedError);
}
extern "C" void CaptureContact(const Registers* regs) noexcept { CaptureStoredContact(regs,false); }
extern "C" void CaptureMissileContact(uintptr_t projectile, uintptr_t target, const float* localPosition) noexcept {
    if(!active.load(std::memory_order_acquire) || !projectile || !target) return;
    const DWORD savedError=GetLastError();
    // r13 already contains the target-local point. The collision producer at
    // 606d47..606d6c subtracts the target translation and applies inverse rotation.
    // 6084a7..6084cb converts it BACK to the missile's zone for the engine effect.
    // Applying that transform here incorrectly feeds zone coordinates to the BVH.
    float spatial[16]{};
    if(!SafeCopy(spatial,localPosition,3*sizeof(float))) {SetLastError(savedError);return;}
    for(int k=0;k<3;++k) if(!std::isfinite(spatial[k]) || std::abs(spatial[k])>=1000000){SetLastError(savedError);return;}
    Registers regs{};regs.rcx=projectile;regs.rdx=target;regs.r8=reinterpret_cast<uintptr_t>(spatial);
    // r9 at the missile entry is not a verified flight segment. Do not use it
    // as an incoming direction; the fleet renderer retains nearest-point fallback.
    CaptureStoredContact(&regs,true);
    SetLastError(savedError);
}
bool PopContact(Contact& out) noexcept {
    if (!TryAcquireSRWLockExclusive(&queueLock)) return false;
    bool present = count != 0;
    if (present) { out = queue[head]; head = (head + 1) % 512; --count; }
    ReleaseSRWLockExclusive(&queueLock);
    return present;
}
