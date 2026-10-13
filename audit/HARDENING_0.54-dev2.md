# 0.54-dev2 — audit resolutions (2026-10-14)

Superseded by dev3: the user reported no effects in the dev2 game test.
See [thread handoff regression](THREAD_HANDOFF_0.54-dev3.md). The checks below
are historical standalone results and did not catch the OS-thread restriction. The original report and
dev1 evidence are historical. The old `audit/repro` harness describes dev1 and
expects its defects; use commit b4b632c to reproduce that state. Current regression
tests are under `tests/` and run through default CTest.

| Finding | Dev2 response | Evidence |
| --- | --- | --- |
| External dependencies outside five profiles | Conservative code/data section gate plus live-code comparison | External callee, getter, vtable, data, unrelated code and unwind-data mutations rejected |
| Unprofiled virtual metadata call | Reviewed RTTI whitelist and direct field reads; no virtual call | Synthetic hostile getter never called; replacement slot refused |
| MinHook thread errors ignored | Upstream 1.3.4 plus explicit failure/handle retention patch | Six injected failures; target bytes unchanged, partial suspension resumed |
| 128 to 32 silent overwrite | Poll drains only output capacity, once per UI frame | All 128 burst entries delivered in order across four frames |
| Stale/session contacts | 250 ms QPC TTL, generation reset, stop API, bridge lifecycle | TTL/reset tests; real LuaJIT with mocked engine transitions |
| Successful NaN projection | Bounds, finite checks, degenerate triangle handling, safe encoding | Seven valid/corrupt fixtures; all 286 installed profiles sampled against brute ray |
| Repeated IO/large cache | Negative cache, per-profile limits, 256 MiB array budget, load counters | Bad profile attempted once; current fleet within limits |
| Partial initialization/registration failures | Publish compatibility after geometry; reset failure state; caught C++ allocations; explicit channel status | Source review, wrong-EXE loader test, native/Lua tests |

Results: CTest16/16; eleven data-only ABI cases including simulated ASLR and
live-code mismatch; 286 unique fleet profiles;
isolated real LuaJIT loader and simulated menu/load/backward-clock bridge test.
Evidence is included in `audit/dev2_evidence/`. Modified EXE fixtures were never
executed or distributed. In-game menu/load, combat stress, all weapon channels
and another real executable remain pending.

The stronger gate sacrifices dev1's broad acceptance of unrelated code changes.
It permits header/resource changes only while reviewed code/data stays unchanged.
This is an explicit safety boundary, not a claim that every micro-patch works.
The historical first-hit visual delay remains a separate open issue.
