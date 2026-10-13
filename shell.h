#pragma once
#include <cstdint>
struct Vec3 { float x,y,z; };
struct ShellProjection { Vec3 point; float distance; uint32_t code, face, tested; };
void BuildSynShell();
bool ProjectSyn(Vec3 point, ShellProjection& result);
bool ProjectSynBrute(Vec3 point, ShellProjection& result);
