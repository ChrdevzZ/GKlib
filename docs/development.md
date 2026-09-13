# Development checks

For current build instructions see [building](building.md). For the fixed
upstream source, compatibility changes and Unreleased history see
[fork changes](../FORK_CHANGES.md).


Ordinary builds require CMake 3.24 and a supported C compiler. GKlib tests also
use C++11. Python 3.9 and Git are required only when developer testing is enabled.
Fortran is needed only by a downstream project that chooses a Fortran link
language, not by an ordinary GKlib configure or build.
Use `GKLIB_BUILD_TESTING`, `GKLIB_BUILD_INTEGRATION_TESTING`, and
`GKLIB_BUILD_DEVELOPER_TESTING` together for the full development suite.

The public API test includes allocation accounting and release checks.
Controlled-failure fixtures separately verify recovery and ownership when
allocation or I/O fails; these are not replaced by successful API calls.

CTest covers API behavior, build/install consumers and optional maintenance
tool regressions. Run the upstream mapping audit explicitly when changing
source provenance; it is not a prerequisite for building or testing a source
archive, which may not contain the recorded Git objects.

```sh
python tools/upstream.py check
git diff --check
```

Review changed declarations, comments and macro layout against the mapped
upstream source. Keep the existing style without bulk reformatting. There
are no text-presence or documentation-layout gates in CTest. To exercise
the documented installation workflows, enable integration testing and run
the installation and consumer tests directly through CTest. The suite also
builds and runs a parent-project consumer; selecting PCRE adds C++ package consumption
independently of whether a Fortran compiler is available.

Focused source regressions use deterministic allocation failures to verify
partial matrix cleanup, DFA and register-pair reallocation ownership, and
mcore/memory transaction boundaries. The regex fixture also requires an
allocation failure to leave the register pair empty, cleanable and retryable,
failed backtracking-stack pushes to retain the previously committed depth and
ownership before cleanup, and constrained-state allocation or node-set copy
failures to return a clean allocation error without registering a partial state.
The mcore/memory fixture requires failed operation-stack growth to
leave pointers, capacities and stack positions unchanged, failed tracked
reallocation to retain the old record, constructor failure to release partial
objects and public marker-boundary rejection to occur before allocator mutation.
It also checks that a rejected free preserves the current pointer and later
variadic arguments, and that rejected mcore deletion, pop and destruction retain
their records, statistics and caller handle. Internal mcore-stack growth must
still update an outer frame's owning record when a nested tracker marker is
active.
Separate fixtures cover binary readers with omitted counts, the quicksort
bottom sentinel and removal of allocation bookkeeping before storage is freed.
The binary-reader fault fixture also forces short reads and verifies that every
reader closes its stream before either a returning error or signal recovery.
The public API fixture distinguishes ordinary `gk_getopt` handling of `W;` from
the exact-match, no-match and missing-argument paths in `gk_getopt_long`.

The Python developer checks also exercise runtime copying, unchanged-file
timestamps, missing and empty inputs, and bounded recovery from a transient
Windows exclusive lock. A lock held beyond the retry budget must still fail.

Use an initialized compiler environment and a separate build/install directory
for each toolchain and ABI. Keep MSVC runtime selection consistent across the
library, dependencies and consumers. Keep producer builds isolated to one
MSYS2 environment. Installed C-library consumption between UCRT64 and CLANG64
can be compatible when architecture, C ABI, UCRT ownership and external
runtimes match; their libstdc++ and libc++ object interfaces do not interchange.

Shared GKlib requires the DLL CRT (`/MD` or `/MDd`). Its `FILE*` and signal
interfaces expose CRT state that cannot be exchanged between separate static
CRT instances. The compile-only CRT fixture checks actual flags, configuration
expressions and an empty standard runtime policy. Static GKlib retains
`/MT[d]` support. See Microsoft's explanation of
[CRT objects across DLL boundaries](https://learn.microsoft.com/en-us/cpp/c-runtime-library/potential-errors-passing-crt-objects-across-dll-boundaries).
Cross-compiling probes compile and link without running target executables;
CTest runtime checks use the configured emulator or explicitly report a skip
when it is absent. Build and link checks must still succeed before a composite
scenario may report that runtime skip. See the [test execution contract](building.md#tests-and-maintenance).

Use the `portable` preset for a Release build with IPO and native CPU tuning
disabled. Use `optimized` when IPO support is required while native CPU tuning
remains disabled. Neither preset changes the library type. Static `AUTO` builds
do not enable IPO unless inherited from an explicit parent CMake policy, while
shared Release-like configurations under `AUTO` probe support. The existing
default, release, and shared presets remain available.

For Windows cross-compiler static consumers, validate object architecture,
calling ABI, and `CMAKE_MSVC_RUNTIME_LIBRARY` separately. Intel runtime libraries
are external inputs selected with `IntelRuntime_ROOT` or `CMAKE_PREFIX_PATH`;
they are never copied into a source package or installed by GKlib. A build tree
with C, C++, and Fortran consumers must validate each final link language.

Ordinary Windows oneAPI consumers use icx or icx-cl. CMake and oneAPI versions
may not describe every driver alias with the same argument style, so compiler
selection remains a toolchain concern. The deprecated dpcpp-cl driver enables
SYCL device compilation, where static GKlib's TLS declarations are unsupported.
Host-only use must disable SYCL for compilation and final linking. These flags
are an explicit consumer choice, never an exported GKlib policy. A shared build
does not make GKlib device-callable.

Keep compiler inventories, local paths, complete logs, and run summaries below
an ignored build directory. Public documentation describes repeatable commands
and expected contracts rather than the outcome of one workstation run.

Do not change the accepted upstream baseline during a check. Review both the
source candidate and the mapping candidate after an upstream update. Deleted
legacy build files remain recorded and must not be restored automatically.

## Build-file organization

The root `CMakeLists.txt` declares project options and performs platform
checks before creating targets. Explain each option immediately above its
declaration, including defaults and relevant ABI or performance effects.
Use two-space indentation, blank lines between responsibilities and two
between independent functions or major sections. Keep generated expressions
and embedded probe source intact. Reusable IPO/link-probe helpers and
installation templates remain in `cmake/`; source lists stay with their
owning `src/`, `include/` and `apps/` directories.

## Coding agents

Shared instructions are in [AGENTS.md](../AGENTS.md). See [agent setup](agents.md)
for discovery and [architecture](architecture.md) for code orientation.
