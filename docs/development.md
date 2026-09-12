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
the installation and consumer tests directly through CTest.

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
execute CTest only when a target runtime or emulator is available.

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
