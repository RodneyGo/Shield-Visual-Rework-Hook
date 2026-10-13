#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
struct FunctionProfile {
    const char* name;
    uint32_t length, hookOffset, unwindSize;
    const char* codeHash;
    const char* unwindHash;
};
struct FunctionMatch {
    uint32_t entry=0, hook=0;
    unsigned matches=0;
    bool unique() const { return matches==1; }
};
bool HashBytes(std::span<const unsigned char> bytes, std::string& result);
FunctionMatch FindFunction(std::span<const unsigned char> image, const FunctionProfile& profile);
