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

# Game-tested native update: 0.54-dev3 (2026-10-14)

This package retains the exact DLL and Lua bridge used in the user-run battle.
The internal version remains 0.54-dev3; no runtime code was changed for packaging.

On X4 9.00 build 611726, both hooks started and both visual addons registered.
The last log snapshot at 230.015 seconds recorded 92,663 captured contacts and
92,660 delivered contacts. All recorded queue-drop, read-fault, expiry and invalid
delivery counters were zero. Capture and delivery snapshots are separate; the
three-event difference is not evidence of loss. All 160 combatants had running
Attack orders. The user reported acceptable performance without FPS measurements.

This is not a complete worst-case benchmark: the range's capital turret/shield
audit had unresolved discrepancies, only four geometries were exercised, and the
normal 96-active-wave budget suppressed some visual requests. Other real game
builds and every weapon/effect combination were not validated by this test.
The historical first-hit rendering delay is not claimed fixed.

Compared with the older 0.53-rc2 distribution, this line adds reviewed executable
section/ABI checks, bounded queue and lifetime handling, geometry validation and
the Lua thread-handoff fix. It does not accept arbitrary patches: executable
code changes still require review; resource/timestamp changes can pass the gate.

Use the DLL together with its matching integration/contact_probe.lua. The full
mod provides fleet_shells.bin, shaders and visual assets. MIT covers SVR code;
bundled MinHook/HDE retain their licenses. Source/build instructions are in README.
