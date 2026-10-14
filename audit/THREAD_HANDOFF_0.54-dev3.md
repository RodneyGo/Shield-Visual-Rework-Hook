# 0.54-dev3 — Lua thread handoff regression (2026-10-14)

The user reported no shield effects with dev2. Its game log accepted the baseline
ABI, found all five profiles and registered 286 surfaces, but contained no hook
installation or armed record. That localizes the failure before capture starts;
the old log alone does not prove which thread executed the missing start call.

An isolated host using the game's LuaJIT reproduced a concrete defect: loading
the DLL on one OS thread and then calling its API sequentially with the same Lua
state on a different thread silently returned nil. Dev2's loader also refused a
subsequent load on another thread. The Lua bridge marked itself started anyway.

Dev3 replaces OS-thread identity with an exclusive native-entry guard. Sequential
handoffs work. Concurrent or reentrant access raises a clear Lua error rather
than silently returning nil or waiting on a recursive lock. The protected inner
Lua call releases the lock before propagating errors; call-frame allocation is
outside the lock. This does not make concurrent access to the same Lua VM safe.
Lua API exports are initialized once. No hook, ABI or geometry policy is relaxed.

The bridge starts polling only after native start returns ACTIVE. A rejected
start disables the bridge once and logs why. Bounded diagnostics now include
thread transitions, first UI update, native start requests/results and MinHook
initialization status. A game restart remains necessary to change the DLL.

Validation: old dev2 fails the new sequential-handoff check; dev3 passes with real
LuaJIT. Tests cover API calls and loader reload across threads, argument/result
preservation, lock release after a Lua error, busy/reentrant rejection, normal
menu/load transitions and nil/disabled start responses. Existing CTest16/16,
eleven ABI cases and directional checks over 286 unique profiles also pass.
Evidence is in `audit/dev3_evidence/`.

Update 2026-10-14: a subsequent user-run battle confirmed hooks, capture and
delivery; see [results and limits](../RELEASE_NOTES.md). At preparation time,
in-game acceptance was pending. The later game log records API thread handoffs,
successful hook installation, armed capture and delivered contacts. The user
accepted dev3 as working. This does not validate every visual effect or another
real game build; the historical first-hit rendering delay remains open.
