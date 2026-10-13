#pragma once
#include "shell.h"
#include <string>
bool ConfigureFleetShells(const std::wstring& path);
bool AddFleetShells(const std::wstring& path);
bool HasFleetProfile(unsigned profile);
bool ProjectFleet(unsigned profile, Vec3 point, ShellProjection& result);
bool ProjectFleetBrute(unsigned profile, Vec3 point, ShellProjection& result);
bool ProjectFleetIncoming(unsigned profile, Vec3 point, Vec3 incoming, ShellProjection& result);
void ReleaseFleetShells();
unsigned FleetProfileCount();
struct FleetCacheStats { uint64_t attempts, failures, bytes, lastLoadMicroseconds; };
FleetCacheStats FleetCacheStatistics();
#ifdef SVR_BVH_BUILDER
bool BakeFleetShells(const std::wstring& input, const std::wstring& output);
#endif
