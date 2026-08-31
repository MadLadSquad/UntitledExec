# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**UntitledExec** (`uexec`) is a standalone, cross-platform C++/C library that wraps process
creation/termination and basic IPC (read STDOUT/STDERR, write STDIN) over `fork`+`execvp` on
Unix and `CreateProcess` on Windows. It is developed as its own repo
(`github.com/MadLadSquad/UntitledExec`) and vendored into UntitledImGuiFramework as a git
submodule under `Framework/Modules/OS/ThirdParty/uexec`. Nothing here depends on the framework —
keep it standalone.

## Build

There is no test suite. Build (and thereby smoke-check) via:

```bash
./ci.sh          # mkdir build && cmake .. -DCMAKE_BUILD_TYPE=RELEASE && MSBuild || make -j
```

or manually:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE
make -j$(nproc)
```

CMake knobs:
- `BUILD_VARIANT_STATIC` — build a `STATIC` archive instead of the default `SHARED` library.
- `UIMGUI_INSTALL` — enable `install()` rules (lib to `lib64/`, headers to `include/uexec`,
  pkg-config to `lib/pkgconfig/`).
- Non-Windows builds link `util`. The build always defines `UIMGUI_UEXEC_SUBMODULE_ENABLED`
  (public) and `MLS_EXPORT_LIBRARY` + `MLS_LIB_COMPILE` (private, for Windows dllexport).

## Architecture

### Three layers

1. **`uexec.cpp` / `uexec.hpp`** — platform-neutral public surface. Defines `uexec::execandwait()`
   and the `uexec::ScriptRunner` class. Every method is a thin dispatcher that calls into
   `InternalUnix::*` or `InternalWindows::*` based on `#ifdef _WIN32`.
2. **`uexecunix.{cpp,hpp}` / `uexecwindows.{cpp,hpp}`** — the two platform backends
   (`InternalUnix`, `InternalWindows`). **Both files are always compiled**: each provides the real
   implementation under its platform's `#ifdef` and a stub (returns `-1`/`false`/no-op) under the
   `#else`. When you change a backend method's signature, update the matching stub in the same file
   or the build breaks on the other platform.
3. **`C/cuexec.{cpp,h}`** — flat C ABI (`uexec_*` functions) wrapping the C++ API.

### The C/C++ ABI bridge (important)

`uexecdata.h` defines `uexec_RunnerData`, a C-ABI struct holding all per-process state (pipe
fds/handles, pid pointer, flags). `ScriptRunner` (in `uexec.hpp`) holds a single
`uexec_RunnerData data` member and nothing else, so the C wrapper does
`reinterpret_cast<uexec::ScriptRunner*>(uexec_RunnerData*)` (see the `cast()` macro in
`cuexec.cpp`). **Do not add other data members to `ScriptRunner`, reorder them, or make it
polymorphic** — that cast relies on `ScriptRunner` being layout-identical to a bare
`uexec_RunnerData`.

### Custom string type

By default `uexecstring` is `std::string`. Define `UEXEC_CUSTOM_STRING` (plus
`UEXEC_CUSTOM_STRING_INCLUDE` naming the header) to substitute another string type — this is how
the framework injects its `FString`. Any custom type must expose `.data()`, `.resize()`,
`.erase()`, `.empty()`, `.size()`, and construct from `const char*`.

### Export macros

`MLS_PUBLIC_API` (defined in `uexecdata.h`) expands to `__declspec(dllexport/dllimport)` on Windows
(gated by `MLS_EXPORT_LIBRARY` / `MLS_LIB_COMPILE`) and to nothing elsewhere. Annotate every new
public function/type with it.

## Runner lifecycle & Unix specifics

`ScriptRunner` usage: `init(args, openStderr, openStdout, openStdin)` → repeatedly poll
`finished()` (and `update()` on Windows) while reading/writing → on completion call
`destroy()` (or `terminate()` then `destroy()` to kill early) → `destroyForReuse()` to reset the
same object for another `init()`. `startable()`/`valid()` report reuse and validity state.

Unix backend caveats worth knowing before touching `uexecunix.cpp`:
- All live child PIDs are tracked in the global `inline std::vector<pid_t>
  uexec_internal_pids_do_not_touch`. A `SIGCHLD` handler installed at `init` marks a dead child's
  slot as `-1` but deliberately does **not** erase it, so owners can detect death via `finished()`.
  Actual removal happens in `destroyForReuse`.
- `execonparent<T>(...)` guards parent-only operations: if any tracked pid is `0` (i.e. code is
  running in a forked child that shouldn't be here) it enters `while (true);` to hard-block rather
  than corrupt the parent's state. Preserve this guard on parent-side functions.

All public methods are tagged `UntitledImGuiFramework Event Safety - Any time` — they are safe to
call at any point in the framework lifecycle.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
