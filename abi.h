#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>
struct MetadataBinding { uintptr_t table, getter; };
// Conservative code/data boundary. Resource and PE-header changes are allowed.
bool VerifyABI(const std::wstring& path, std::span<const unsigned char> live,
               uintptr_t base, std::vector<MetadataBinding>& bindings);
