# MinHook 1.3.4 with SVR thread-error handling

Upstream: https://github.com/TsudaKageyu/minhook/releases/tag/v1.3.4
Original downloaded hashes are recorded in `UPSTREAM.json`. Original copyright
and license notices remain in the sources and LICENSE.txt.

SVR changes in `src/hook.c` and `include/MinHook.h`:

- Fail with `MH_ERROR_THREAD_FAILURE` on enumeration, OpenThread, SuspendThread,
  GetThreadContext or SetThreadContext failure before writing the hook patch.
- Retain thread handles until resume. Allocate tracking storage before suspension.
- Suspend all enumerated threads before processing instruction pointers.
- Unwind partial suspension by resuming only successfully suspended handles.

The SVR caller deliberately never removes/uninitializes created hooks following
a failed installation. An earlier thread's IP might already refer to an allocated
trampoline. The pinned DLL and trampolines remain alive until process exit.

This does not eliminate races with threads created after enumeration. Resume
failure and arbitrary native plugins modifying the same code remain limitations.
The six failure-injection tests run in standalone processes, not inside X4.
