#pragma once
#include <Windows.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include "abi.h"

// Layout shared with entry.asm. No assumed C++ signature of the game function.
struct Registers {
    uint64_t rax, rcx, rdx, r8, r9, r10, r11, rsp;
    unsigned char xmm[6][16];
};
static_assert(offsetof(Registers, xmm) == 64);
static_assert(sizeof(Registers) == 160);
struct Contact {
    uint64_t sequence, ticks;
    DWORD thread, valid;
    Registers regs;
    // Raw snapshots; meanings are deliberately not asserted before runtime tests.
    unsigned char context[32], target[32], spatial[64], extra[16], stack[64];
    uint64_t contextId, targetId;
    char contextMacro[128], targetMacro[128];
    unsigned metadataValid;
    // Incoming collision-segment direction in the contact's target-local frame.
    // Only populated for two full-function-validated producer call paths, never from r9 normal.
    float incoming[3];
    unsigned directionSource;
    unsigned missileContact;
};
struct Stats { uint64_t seen, captured, dropped, faults, limited; };
extern "C" void ProbeEntry();
extern "C" void* ProbeTrampoline;
extern "C" void CaptureContact(const Registers*) noexcept;
extern "C" void MissileProbeEntry();
extern "C" void* MissileProbeTrampoline;
extern "C" void CaptureMissileContact(uintptr_t projectile, uintptr_t target, const float* localPosition) noexcept;
void SetCapture(bool enabled) noexcept;
void ResetCapture() noexcept;
bool ContactFresh(const Contact&) noexcept;
bool PopContact(Contact&) noexcept;
Stats CaptureStats() noexcept;
bool SafeCopy(void* dst, const void* src, size_t length) noexcept;
void SetMetadataImageRange(uintptr_t base, size_t size) noexcept;
void SetMetadataBindings(const MetadataBinding* bindings, size_t count) noexcept;
bool ReadContactDirection(const Registers&, const unsigned char* spatial, float* incoming, unsigned& source) noexcept;
bool CopyMSVCString(const void* source, char* out, size_t capacity) noexcept;

void ConfigureDirectionReturns(uintptr_t first, uintptr_t second, uintptr_t innerFirst, uintptr_t innerSecond) noexcept;
