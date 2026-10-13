# Shield Visuals Rework — native component

Experimental native component **0.54-dev1**, tested for startup on X4 9.00 build 611726. This is not a new full-mod release.

This repository builds `svr_contact_probe.dll`. It is not a standalone X4 mod
and does not contain the complete mod, ship meshes, textures or game files.
The optional prebuilt candidate is in `binaries/0.54-dev1/`; its SHA-256 matches
the DLL used for the baseline game test. Build from source if you prefer. Use it to inspect the native code or build your own DLL
for an existing Shield Visuals Rework installation.

## What the DLL does

The game-side Lua script loads the DLL with `package.loadlib`. The DLL uses
MinHook to install in-process hooks in two contact-handling paths in the
supported X4 executable. It copies target IDs, projectile and target macro
names, target-local contact positions and, where verified, incoming direction
into bounded queues. The original game code continues through the trampolines.
It does not replace the damage calculation or track every projectile in flight.

The DLL also projects contacts onto prepared shield surfaces. Lua reads the
results and passes effect requests to Mission Director. The game renderer and
the mod's shaders draw the effects; the DLL does not implement a renderer.

`fleet_shells.bin`, supplied with the full mod, is data rather than executable
code. Its `SVR_FL08` format stores profile IDs, mesh vertices and triangles,
coordinate bounds/quantization and prebuilt bounding-volume hierarchies (BVHs)
for surface queries. Registered addons can supply their own geometry files.
These meshes are visual surfaces, not new physical collision barriers.

## Requirements

- Windows x64.
- Visual Studio 2022 or Visual Studio 2022 Build Tools, with the **Desktop
  development with C++** workload: MSVC v143 x64/x86 tools (including the x64
  MASM assembler) and a Windows 10 or Windows 11 SDK.
- CMake 3.20 or later, available in your terminal's PATH. CTest comes with CMake.

The compiler must support C++20. This build uses Windows APIs, MSVC structured
exception handling and MASM; MinGW/Linux builds are not supported by this project.
MinHook sources and their notices are bundled under `third_party/minhook`.
The DLL links the MSVC runtime statically and uses the Windows SDK's `bcrypt`.
No dependency download occurs during CMake configuration.

You do **not** need X4, Blender, X Tools, Python, a Lua development package or
the game's source code to compile the DLL and run the included standalone tests.
The game-provided `lua51_64.dll` is resolved at runtime and is not redistributed.

## Build

Extract the archive, open PowerShell in the directory containing
`CMakeLists.txt`, then run each command and check that it succeeds:

```powershell
cmake -S . -B out -G "Visual Studio 17 2022" -A x64
cmake --build out --config Release
ctest --test-dir out -C Release --output-on-failure
```

The output is **`out/Release/svr_contact_probe.dll`**. Build the full default
target before running CTest, so that both test executables exist.
To build only the DLL instead:

```powershell
cmake --build out --config Release --target svr_contact_probe
```

The two standalone tests are `native_capture` and `missile_capture`. They
exercise the observers against synthetic functions/data, including argument
forwarding and missile contact capture. They hook their own test processes;
they do not start or attach to X4. They do not establish compatibility with
another game version, prove security, test the full fleet or validate rendering.

If Visual Studio is not found, install the C++ workload and try a fresh
Developer PowerShell for VS 2022. If selecting a different generator/compiler,
use a new output directory such as `out-vs2022` rather than reusing its cache.
For errors mentioning `ml64`, check the MSVC x64 tools/MASM installation.
The DLL may be locked while X4 is running: close X4 before replacing it.

## Use your build

1. Install the complete compatible Shield Visuals Rework mod first.
2. Fully close X4 and back up the existing DLL outside the extension folder.
3. Copy your built DLL to
   `X4 Foundations/extensions/shield_visuals_rework/native/svr_contact_probe.dll`.
4. Keep `fleet_shells.bin` and the remaining files from the matching mod release.
   Rebuilding this DLL does not require rebuilding the geometry database.
5. Start X4 with Protected UI Mode disabled and the mod enabled.

This archive is not itself installed under `extensions`. Copy only the built
DLL when replacing the native component. No injector application is required:
the mod's Lua bootstrap loads it. Nevertheless, the hook modifies executable
code in process memory. It does not patch the X4 executable on disk.

## Supported executable and update policy

Current target: **X4 9.00, build 611726, Windows x64**, with executable SHA-256:

```text
19750a6563889a970f434b5566eb396c6b2dc29ff814bd3e336f838176ad6891
```

The full EXE hash is logged, but is no longer an acceptance gate. The resolver
searches executable functions using full-body SHA-256 and unwind fingerprints.
Each required profile must match exactly once. Contact and missile paths are
enabled independently; direction extraction needs its own matching profiles.

These are conservative fingerprints, not relocation-masked signatures. Changes
to relative call operands may still require a new profile. Matching functions
do not prove that external callees, virtual tables, object layouts or stack
assumptions remain valid. Another real game build has NOT been validated.
Do not regenerate fingerprints for an unknown build without reviewing those
dependencies. This candidate is not a guarantee of compatibility with updates.

Validation: both synthetic capture tests and nine offline EXE mutation cases
passed. An actual game log on the baseline build confirms five unique profiles,
both hooks installed successfully, and base/addon geometry registration. The
test did not independently establish coverage of every weapon class.

`compatibility_audit.exe <path-to-X4.exe>` reads an executable as data and reports
profile matches; it does not execute it or install hooks. Its success alone
does not establish ABI compatibility. Build this tool with the default target.

Compiling successfully does not make a new X4 version compatible. A native
bug can crash the game; the compatibility checks are not a security guarantee.
Protected UI normally blocks this native-loading path. Disabling it permits
native code to run with the game's process permissions.

## Source map and review scope

| File | Role |
| --- | --- |
| `probe.cpp` | Lua exports, executable checks, hook installation, queue draining, addon registration and logging |
| `compatibility.cpp` / `compatibility.h` | Bounded PE function matching |
| `compatibility_profiles.h` | Fingerprints of the baseline functions |
| `compatibility_audit.cpp` | Read-only executable profile inspection |
| `capture.cpp` / `capture.h` | Contact copying, bounded queue, object metadata and verified direction extraction |
| `entry.asm` | Register preservation and forwarding to the original code via MinHook trampolines |
| `fleet_shell.cpp` / `fleet_shell.h` | FL08 loading, BVH queries, surface projection and coordinate encoding |
| `shell.h` | Shared vector/projection types; no embedded ship geometry |
| `selftest.cpp` | Synthetic ordinary-contact and forwarding checks |
| `missile_selftest.cpp` / `missile_fixture.asm` | Synthetic missile observer checks |
| `third_party/minhook` | Hooking library and bundled disassembler, with original notices |

Runtime file access includes reading the executable for hashing, reading base
and registered-addon geometry, and writing diagnostic logs in the mod's `logs`
directory. Detailed contact records are enabled by a file named
`diagnostics.enabled` in the main extension directory; basic startup/status
logging does not require that file. There is no networking, telemetry upload,
updater, child-process launcher or save-file editing implemented in these sources.
This description is a guide for source review, not an independent security audit.

The runtime translation units and their headers are copied without edits from
the working native source. The export's `CMakeLists.txt` retains the DLL's build
settings and dependencies, but omits legacy embedded-Syn tests, old loader
fixtures and offline geometry tools that are unnecessary for compiling it.
No old generated ship mesh is distributed in this source package.

## Provenance and reproducibility

`SOURCE_INFO.json` records the release DLL hash and hashes of the source inputs.
`FILES.sha256` covers the distributed files, excluding the manifest itself.
These identify the supplied files; hashes alone are not a security audit or
proof that an arbitrary binary was built from a particular source tree.

A local build is not promised to be byte-for-byte identical to the release
DLL. Compiler/SDK versions, paths, linker metadata and timestamps can change
the binary. A different DLL hash alone does not demonstrate different behavior.
For inspection, review the sources and build your own copy.

## Publishing and licenses

The extracted directory can be used as a GitHub repository root. Commit the
source, documentation, manifests and third-party notices. `.gitignore` excludes
build output. No automatic upload, update or installation is performed by these sources.

MinHook and its bundled disassembler retain their original licenses in
`third_party/minhook/LICENSE.txt` and source headers; see
`THIRD_PARTY_NOTICES.md`. No project-wide open-source license has been selected
for the SVR-authored code in this export. The author should choose and add a
license before describing the repository as open source; the third-party
license does not automatically license the rest of the project.
