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
