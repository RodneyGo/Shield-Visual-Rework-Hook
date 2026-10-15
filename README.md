**Original release DLL — 0.53-rc2 — SHA256:** `2ac1b1033ffca240cc741a8227b366c7dc495407699d20927c94b1d9fbc25f94`

**Latest development DLL — 0.54-dev3 — SHA256:** `e8cf8e8e39bf00369a34c0ff20815e6b153680cc44b835e6f331b8da4cfcced0`

## DLL version notice — 15 October 2026

Two DLL versions are relevant to the current release:

- **0.53-rc2 — original release DLL:** 272,896 bytes, originally shipped with
  SVR 0.66 and now included in the **corrected SVR 0.70 full archive**. It uses
  strict executable checks for X4 9.00 build 611726. Even small executable
  updates can require a native update.
- **0.54-dev3 — latest development DLL:** 370,688 bytes. It includes the newer
  compatibility checks, queue/validation improvements and Lua thread-handoff fix.
  It was tested in a user-run battle, but is **not the DLL selected for the
  current corrected 0.70 release**. Its prebuilt file and source remain here
  for inspection; this is not a recommendation to replace the release DLL.

The initial 0.70 archive mistakenly contained 0.54-dev3. The corrected archive
uses 0.53-rc2 with the same 0.70 visuals and Lua bridge. That exact combination
has not received a new in-game acceptance test. Future native changes will be
announced explicitly; a visual update does not imply a DLL upgrade.

The hashes above identify **svr_contact_probe.dll itself**, not a ZIP archive,
and are not antivirus verdicts. Extract the DLL and check it in PowerShell:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath "C:\path\to\svr_contact_probe.dll"
```

The source at the repository root builds the **0.54-dev3 development line**, not
0.53-rc2. Use the corresponding historical source package for the original DLL.
Locally rebuilt binaries may have different hashes because of build inputs/tools.

# Shield Visuals Rework — native component

Development source: **0.54-dev3**, tested on X4 9.00 build 611726.
The exact tested DLL is retained. See [release notes](RELEASE_NOTES.md) for game-test results and limits.
This is not a new full-mod release or a guarantee of compatibility with updates.

This repository builds `svr_contact_probe.dll`. It contains source, tests and an
optional prebuilt candidate in `binaries/0.54-dev3`. It contains no X4 executable,
game LuaJIT DLL, ship geometry or textures. The older binaries and audit are
kept as historical evidence; use the version matching the reviewed source.

Dev2 produced no shield effects in the user test. Dev3 removes its incorrect
OS-thread affinity check and requires a successful native start before polling.
The old failure and the corrected sequential thread handoff were reproduced in
an isolated real LuaJIT host. A subsequent user-run battle confirmed active
capture and delivery. See [game-test results](RELEASE_NOTES.md) and
[dev3 regression fix](audit/THREAD_HANDOFF_0.54-dev3.md).

## What it does

The mod's Lua bridge loads this DLL with `package.loadlib`. MinHook installs
in-process observers in the supported game's contact and missile paths. The
observers copy IDs, macro names, target-local hit positions and verified incoming
directions. The original game code continues through trampolines. This does not
replace damage calculations, simulate projectiles or render graphics.

The DLL reads metadata fields directly through approved definition-interface
bindings. It does not call a game's virtual metadata function. Contacts enter a
bounded queue; Lua requests surface projections and passes effect requests to
Mission Director. Custom shaders provide the visual effects.

`fleet_shells.bin` is non-executable data supplied with the full mod. Its SVR_FL08
format contains profile IDs, vertices, triangles, quantization and prebuilt BVHs.
Addons can register additional files. These surfaces are not collision barriers.

## Build

Requirements: Windows x64, Visual Studio 2022/Build Tools with **Desktop
development with C++**, MSVC v143, x64 MASM, a Windows SDK, and CMake 3.20+.
The code uses C++20, MSVC SEH and Windows APIs. MinGW/Linux are not supported.
MinHook is bundled; configuration performs no downloads. The MSVC runtime is
linked statically. You do not need X4, Python, Blender or X Tools to build and run
the 16 default standalone tests.

```powershell
cmake -S . -B out -G "Visual Studio 17 2022" -A x64
cmake --build out --config Release
ctest --test-dir out -C Release --output-on-failure
```

Output: `out/Release/svr_contact_probe.dll`. To build only the DLL, add
`--target svr_contact_probe` to the build command. Build all default targets
before running CTest. If MSVC is not found, check the C++ workload and use
Developer PowerShell for VS 2022. Use a new build directory when changing tools.

Tests hook only their own synthetic processes; they do not start or attach to X4.
The optional loader and fleet tools require your own game installation/data and
are not part of default CTest. To test the actual Lua ABI and bridge lifecycle:

```powershell
.\out\Release\loader_selftest.exe "C:\path\to\X4 Foundations\lua51_64.dll" `
  ".\out\Release\svr_contact_probe.dll" ".\integration\contact_probe.lua" `
  ".\tests\bridge_lifecycle.lua"
```

Run this from the repository root. It does not launch X4 or install game hooks. Successful compilation is not a security audit or
an in-game acceptance test.

## Install the candidate

1. Install the full Shield Visuals Rework mod and close X4 completely.
2. Back up both `native/svr_contact_probe.dll` and `ui/contact_probe.lua` outside
   the installed extension folder.
3. Copy the built DLL to
   `X4 Foundations/extensions/shield_visuals_rework/native/svr_contact_probe.dll`.
4. Copy `integration/contact_probe.lua` to the same extension's
   `ui/contact_probe.lua`. This matching bridge checks the native start result
   and uses the session-reset API.
5. Keep the geometry, shaders and other files from the full mod. Start with
   Protected UI Mode disabled. To roll back, restore both backed-up files.

Do not install this entire repository under extensions. There is no separate
injector or automatic updater: Lua loads the DLL. Hooks modify executable memory
inside X4; the executable on disk is not patched. No game saves are written by
this component. Source files do not certify an arbitrary downloaded binary.

## Compatibility policy

Baseline: X4 9.00/611726 Windows x64, executable SHA-256:

```text
19750a6563889a970f434b5566eb396c6b2dc29ff814bd3e336f838176ad6891
```

Dev2 deliberately tightens dev1's incomplete five-function gate. The full-file
hash remains diagnostic, but all PE sections except `.rsrc` must match reviewed
hashes, sizes, addresses and flags. Loaded executable code must also match a
private ASLR-adjusted reference. The five function/unwind profiles are checked
after that. Metadata bindings require reviewed RTTI and the known getter hash.

Header timestamps and resource contents may change without disabling the DLL.
Code/data changes, including changes elsewhere in a small patch, currently cause
refusal. This conservative boundary closes the demonstrated external-callee gap;
it does **not** solve general cross-build compatibility. Another real build has
not been tested. Never update hashes blindly to accept an unknown executable.

Read-only inspection (the file is treated as data, never executed):

```powershell
.\out\Release\compatibility_audit.exe "C:\path\to\X4.exe" --abi
```

Omitting `--abi` prints the individual function matches, which alone do not
authorize runtime hooks. In-memory code changes by another native plugin can
also cause the stricter runtime check to refuse capture.

## Audit fixes and remaining limits

See [dev2 changes and evidence](audit/HARDENING_0.54-dev2.md) and the
[original dev1 audit](audit/REVIEW_2026-10-14.md). Confirmed fixes include
queue overwrite, stale contacts, session reset, invalid geometry, failure
caching, direct metadata reads and explicit partial-hook status.

MinHook 1.3.4 has local fail-closed thread handling; see
`third_party/minhook/SVR_CHANGES.md`. A snapshot is not a process-wide atomic
freeze. New-thread races, resume failures, mid-function unwind/upper AVX state,
direct save-load lifecycle and real new game builds remain validation limits.
The historical first-hit rendering delay has not been demonstrated fixed.

Contacts older than 250 ms are dropped; queue and visual budgets remain bounded.
Projection data is loaded synchronously on first use. Mesh arrays have a 256 MiB
budget and failures are cached until cache reset/process restart. Current data
limits are 65,536 vertices / 131,072 faces per profile and coordinates within
one million metres per axis. Logs include bounded summary counters; raw capture
diagnostics are optional and may contain game memory fragments and addresses.

## License

SVR-authored code and documentation: [MIT](LICENSE), Copyright 2026 RodneyGo.
MinHook/HDE retain their original licenses; see [third-party notices](THIRD_PARTY_NOTICES.md).
