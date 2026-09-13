# Building GKlib

This reference describes the current fork. For the fixed upstream comparison
and removed legacy commands, see [fork changes](../FORK_CHANGES.md).

## Requirements

CMake 3.24 or newer, a supported C compiler for the C99 sources, and a CMake
build tool are required. Tests additionally require a C++11 compiler. Ninja
presets require Ninja. MSVC can compile these sources without a strict C99 mode.
Python 3.9 and Git are developer-only requirements. Fortran is needed only by
optional downstream mixed-language tests, not by the library itself.

## Source preparation

```sh
git clone https://github.com/ChrdevzZ/GKlib.git
cd GKlib
```

The modern build changes are currently Unreleased. A remote revision containing
only the fixed upstream baseline is not equivalent to this modified checkout.
The `src/`, `include/` and `apps/` layout already exists in that baseline;
CMake now manages their targets and interfaces separately. All generated files
belong in the build directory.

## Build configuration


Select the compiler, generator, toolchain file, installation prefix and MSVC
runtime through standard CMake variables before the first configuration. Use
separate binary and installation directories for incompatible toolchains or
ABIs. Debug, Release, RelWithDebInfo and MinSizeRel are supported.

For a multi-config generator:

```sh
cmake -S . -B build/multi -G "Ninja Multi-Config"
cmake --build build/multi --config Release --parallel 2
ctest --test-dir build/multi -C Release --output-on-failure
cmake --install build/multi --config Release --prefix /path/to/prefix
```

`BUILD_SHARED_LIBS` supplies the initial shared-library default. Project-specific
options take precedence; no option forces unrelated parent targets to use the
same settings. Programs, tests and install rules default on only at top level.


Top-level builds enable programs, tests, and install rules by default.
Subproject builds leave them disabled. `GKLIB_BUILD_SHARED_LIBS` inherits
`BUILD_SHARED_LIBS` when that variable is set and otherwise defaults to
static.

| Option | Default | Values |
| --- | --- | --- |
| `GKLIB_BUILD_SHARED_LIBS` | inherited or `OFF` | `ON`, `OFF` |
| `GKLIB_BUILD_PROGRAMS` | top-level | `ON`, `OFF` |
| `GKLIB_BUILD_TESTING` | top-level | `ON`, `OFF` |
| `GKLIB_INSTALL` | top-level | `ON`, `OFF` |
| `GKLIB_BUILD_INTEGRATION_TESTING` | `OFF` | Isolated package/policy/install regressions |
| `GKLIB_BUILD_DEVELOPER_TESTING` | `OFF` | Maintenance tool regressions; requires `GKLIB_BUILD_TESTING` |
| `GKLIB_ASSERTIONS` | `AUTO` | `AUTO`, `ON`, `OFF` |
| `GKLIB_ASSERTIONS_EXPENSIVE` | `OFF` | `ON`, `OFF` |
| `GKLIB_DEBUG` | `AUTO` | `AUTO`, `ON`, `OFF` |
| `GKLIB_IPO` | `AUTO` | `AUTO`, `ON`, `OFF` |
| `GKLIB_NATIVE_OPTIMIZATION` | `OFF` | `ON`, `OFF` |
| `GKLIB_OPENMP` | `OFF` | `ON`, `OFF` |
| `GKLIB_GPROF` | `OFF` | `ON`, `OFF` |
| `GKLIB_REGEX_BACKEND` | `AUTO` | `AUTO`, `SYSTEM`, `GKREGEX`, `PCRE` |
| `GKLIB_USE_GKRAND` | `OFF` | `ON`, `OFF` |
| `GKLIB_NO_X86` | derived from target architecture | `ON`, `OFF` |
| `GKLIB_THREAD_LOCAL_STORAGE` | `ON` | `ON`, `OFF` |
| `GKLIB_SANITIZERS` | empty | e.g. `address;undefined` |
| `GKLIB_WARNINGS_AS_ERRORS` | `OFF` | `ON`, `OFF` |

`GKLIB_WARNINGS_AS_ERRORS` promotes the selected compiler's project warnings
to build errors. It is an opt-in diagnostic policy; retained upstream code can
still produce deprecated-declaration warnings, including the bundled regex
implementation's C99-compatible non-prototype definitions. Keep it off for
ordinary dependency builds, or review those diagnostics with the selected
compiler. Configuration success does not certify a warning-free build.

`AUTO` assertions and debug instrumentation follow the active build
configuration, including multi-config generators. Target architecture detection
sets the default for `GKLIB_NO_X86`; enable it to disable x86-specific paths.
Optimization, diagnostics, OpenMP, profiling, sanitizers, and assertion policy
are attached to the GKlib target and do not modify global compiler flags.
Thread-local storage covers both error-recovery state and the allocation tracker.
Disabling it makes both states process-global and changes concurrency safety.

With `AUTO`, ordinary static archives do not enable IPO on their own unless a
parent project supplies an explicit CMake IPO policy. Shared Release,
RelWithDebInfo, and MinSizeRel builds probe support before enabling IPO. An
explicit `ON` fails configuration when the selected toolchain cannot link an
IPO program. Use the `portable` preset when output must be consumed across
compatible compiler installations.

On Windows, shared GKlib requires `/MD` or `/MDd`. The public `FILE*` and signal
interfaces require the caller and DLL to share one CRT instance; two `/MT`
instances are not interchangeable, even with the same compiler version. CMake
rejects static CRT settings for shared GKlib in each enabled configuration.
Static GKlib continues to support `/MT` and `/MTd`. Consumers must preserve the
CRT contract and release GKlib-owned memory with GKlib's allocation functions.



Per-configuration parent IPO settings take precedence over global IPO policy.
Without either, Debug and custom configurations do not enable IPO under AUTO.
Only configurations available in the current build are checked. Each check
configures and links a small CMake project with that configuration's compiler,
CRT, compile/link flags and target options. Static checks link an archive into
a caller; shared checks link a shared library. Target programs are never run.
Required IPO failures report a diagnostic log; default shared AUTO disables
only the configuration whose check failed. Results are cached in this build
tree per project, configuration, link type and effective input signature. An
unchanged reconfigure reuses the result and log. Disabled IPO needs no probe.
This checks the selected toolchain and options, not arbitrary dependency graphs
or another compiler's IPO object format.

Native tuning is unavailable for cross-compilation or universal multi-architecture
builds. Quote sanitizer lists, for example `-DGKLIB_SANITIZERS="address;undefined"`.

Sanitizer checks require actual instrumentation and its final-link runtime.
An ignored compiler switch is not support. Native MSVC supports AddressSanitizer
only; clang-cl and Intel LLVM are checked separately. MSVC-style frontends do
not implement gprof's `-pg` interface and reject `GKLIB_GPROF=ON`.

Compile-and-link feature checks use an executable and the active configuration,
including configuration-specific flags and the selected custom linker. Common
diagnostics for ignored or unsupported options make a requested capability fail
even when the compiler exits successfully. Each check restores its temporary
state and caches a result only for the complete effective input signature.

For Windows MSVC-ABI Clang and Intel LLVM ASan, the build records the runtime
libraries selected by the compiler for the effective architecture and CRT.
Static installation consumers recover those libraries through their compiler
SDK, `CompilerRuntime_ROOT` or `CMAKE_PREFIX_PATH`; no producer SDK paths are
exported. Use a compatible producer runtime SDK, including its DLLs. A
completed shared library requires runtime deployment, not the static development
SDK. Sanitized archives have
additional runtime constraints beyond the ordinary C ABI.

ASan final links using LLD disable string tail merging to avoid overlapping
instrumented globals, following the [LLVM workaround](https://github.com/llvm/llvm-project/pull/74207).
The link interface follows the final language and target
linker selection, including explicit `LINKER_TYPE` overrides; ordinary
`link.exe` links do not receive LLD-only switches. Intel Windows IPO links use
the Intel driver and its LLD path. This workaround does not change ordinary
unsanitized builds or make IPO archives portable between compiler toolchains.

Build-tree tools and tests copy the selected ASan DLL beside their executables.
On Windows, a bounded retry handles a short permission or sharing conflict
during that copy; missing inputs and persistent failures remain fatal.
Installed packages do not redistribute compiler runtimes. Deploy compatible
ASan and other required runtime DLLs explicitly; an unrelated SDK on `PATH`
must not be used as a substitute. Debug CRT support is checked for the actual
compiler configuration and is not assumed from Release ASan support.

For MSVC AddressSanitizer builds, prefer `RelWithDebInfo`: without debug
information MSVC emits [C5072](https://learn.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warning-c5072?view=msvc-170).
Warnings-as-errors is an opt-in diagnostic policy; inherited source warnings on
some compilers can prevent a build even when the default configuration succeeds.

GKlib Ninja presets differ from METIS: `default` is Debug, `release` is Release,
and `shared` is shared Debug. `portable` and `optimized` inherit Release, disable
native tuning, and select IPO OFF or ON respectively without changing library
type. Use untracked `CMakeUserPresets.json` for local compiler paths.

## Dependencies

The root `CMakeLists.txt` selects the regex backend. AUTO prefers a linkable system
POSIX regex implementation, falling back to bundled GKREGEX. SYSTEM requires
its header and functions; GKREGEX selects the bundled implementation; PCRE
requires the PCRE POSIX wrapper and its underlying library. Explicit requests
fail rather than silently falling back. `GKLIB_OPENMP=ON` requires OpenMP.
For static installations, the package records the producer's OpenMP runtime
library names and resolves those exact runtime dependencies without enabling a
consumer language or adding another compiler's OpenMP compile flags. This keeps
pure Fortran package consumers viable. Set `OpenMP_ROOT` or `CMAKE_PREFIX_PATH`
to a compatible producer SDK when its runtime libraries are outside normal
search paths. A shared GKlib has already resolved its private OpenMP link and
requires runtime DLL deployment rather than the producer development SDK.

Runtime discovery follows the consumer's active configurations and imported
configuration mappings. An unused installed Debug configuration does not require
its runtime SDK in a Release-only consumer; an explicit mapping still requires
the selected producer runtime.

With CMake 3.30 or newer, an MSVC producer can select the LLVM runtime through
FindOpenMP's standard
[`-DOpenMP_RUNTIME_MSVC=llvm`](https://cmake.org/cmake/help/latest/module/FindOpenMP.html#input-variables)
input. Static GKlib packages then require the matching `libomp.lib` or
`libompd.lib` development library, and programs still need the corresponding
runtime DLL. GKlib does not install that SDK or DLL. Microsoft documents
`/openmp:llvm` as experimental and states that its required DLLs are not
redistributable, so it is not supported for production deployment; see the
[MSVC `/openmp` reference](https://learn.microsoft.com/en-us/cpp/build/reference/openmp-enable-openmp-2-0-support?view=msvc-170).

A PCRE-enabled package records the static or shared linkage established while
configuring the C producer. C and C++ consumers continue to validate the headers
and link. A pure Fortran consumer performs a compile-and-link-only `BIND(C)`
check of the required PCRE POSIX symbols. The probe is not executed. Without
C/C++ or a recorded producer linkage, discovery fails clearly instead of
guessing `PCRE_STATIC`.
When a parent already provides `PCRE::POSIX`, GKlib preserves an explicit
`PCRE_LINKAGE=STATIC|SHARED` value or infers it from a concrete `PCRE::POSIX` or
canonical `PCRE::pcreposix` target. An opaque interface graph remains valid for
non-install builds. Set `PCRE_LINKAGE` explicitly before an installing build
because an exported package must record which linkage its consumers should use.
Optional integration tests require an OpenMP implementation even when the main
build has OpenMP disabled.

When integration testing is enabled and a Fortran compiler is discoverable, the
suite also tests pure Fortran package consumers. Set `CMAKE_Fortran_COMPILER`
explicitly when it is outside the initialized environment. Selecting PCRE adds
the corresponding consumer regression using that SDK; ordinary builds do not
discover or require Fortran.

Intel runtime lookup on Windows uses an external SDK as described below.
GKlib is independently buildable and has no dependency on a METIS checkout.

## Installation and consumption

```sh
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel 2
ctest --test-dir build/release --output-on-failure
cmake --install build/release --prefix /path/to/prefix
```

```cmake
find_package(GKlib CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE GKlib::GKlib)
```

Set `CMAKE_PREFIX_PATH` to the installation prefix. Alternatively use
`add_subdirectory(ext/GKlib)` before linking the same target. Standalone builds
include all eleven programs; subdirectory builds leave programs, tests and
installation to the parent. Install paths follow GNUInstallDirs, with Windows
DLLs in the binary directory. The package can move with its installation prefix.

Use `cmake --build build/release --target package_source` to create source
archives. After an install, `cmake --build build/release --target gklib-uninstall`
removes only files in GKlib's own installation records. Records preserve actual
names, Debug postfixes, configurations, components, prefixes, and `DESTDIR`.
Files installed by a parent project are not selected by basename. Keep the
build tree's `install-manifests/` directory for uninstall support. If multiple
prefixes were installed, select one:

```sh
cmake -DGKLIB_UNINSTALL_PREFIX=/path/to/prefix -P build/release/cmake/uninstall.cmake
```

Uninstall rejects malformed records and paths outside the recorded prefix.
Use install destinations within that prefix for the dedicated uninstall target.

Supply the same `DESTDIR` for staged UNIX installs. Components are
`GKlib_Runtime`, `GKlib_Development`, and `GKlib_Applications`.



## Platforms and mixed compilers

The shared-library build exports the complete C API, including getopt globals
and thread-local error state. Both C and C++ consumers should include
`<GKlib.h>` and link `GKlib::GKlib`.

An IntelLLVM-produced static archive consumed with another compiler family
requires the matching external Intel C runtime development libraries. Select
them with `IntelRuntime_ROOT` or `CMAKE_PREFIX_PATH`. GKlib does not package or
install those Intel libraries or DLLs. The lookup validates target architecture
and selects the MD, MDd, MT, or MTd libraries matching the producer's MSVC
runtime policy. For an Intel IPO archive, the extra final-link option is emitted
only for IntelLLVM C, C++, or Fortran link languages; this does not make the
archive compatible with a foreign IPO linker.

Static consumers must use compatible object architecture, calling ABI, CRT
policy, and public type configuration. Shared libraries provide a compiler
boundary, but target architecture and public ABI still have to match.

Shared GKlib requires the DLL CRT (`/MD[d]`) because its public FILE and signal
interfaces cross the DLL boundary. Static GKlib supports `/MT[d]`. Matching
compiler brands alone do not establish compatible CRT ownership. Use library
allocation/free functions for library-owned memory. Known Intel DLL dependencies
must be available at runtime; a linked shared library does not require the
consumer to install Intel static development libraries.

A parent project can select compilers separately for languages that CMake's
platform modules allow together. On Windows, CMake 3.24 and 4.3 reject
mixing MSVC with Clang or another CL-compatible compiler ID across C and C++
during language initialization; this is a CMake toolchain restriction, not a
finding that ordinary ABI-compatible objects cannot interoperate. This project
does not bypass that platform check. C and compatible Fortran compilers can
still be selected independently. See the corresponding
[CMake 3.24](https://gitlab.kitware.com/cmake/cmake/-/blob/v3.24.0/Modules/Platform/Windows-Clang.cmake#L151-166)
and [CMake 4.3](https://gitlab.kitware.com/cmake/cmake/-/blob/v4.3.0/Modules/Platform/Windows-Clang.cmake#L180-202)
platform checks.

Distinct C producers require separate builds and installed packages. Keep each
MSYS2 producer build inside one environment. MINGW64 uses MSVCRT; UCRT64 and
CLANG64 both use UCRT, so compatible installed C-library consumption between
the latter two is possible when architecture, calling ABI, runtime ownership
and external dependencies match. Their libstdc++ and libc++ object interfaces
differ, and IPO/object compatibility must be established separately. See
[development checks](development.md).

### Public templates

The existing `GK_MK..._PROTO` macros declare caller-owned instances without
GKlib DLL attributes. The corresponding `GK_MK..._PROTO_EX` variants take an
additional final export-attribute argument for instances owned by a library.
GKlib's own declarations use `GKLIB_EXPORT`; consumers do not need to redefine
an ambient template export macro.

## Tests and maintenance

Python 3.9 or newer and Git are maintenance dependencies, not ordinary build
requirements. The integration suite additionally needs an OpenMP implementation.

Nested build tests inherit a bounded set of toolchain, architecture, CRT and
compiler/linker settings through a generated initial cache. They use the active
CTest configuration; scenarios that deliberately select another configuration
state it explicitly. The cache does not enable additional project languages,
including in Fortran-only installed-package consumers.

Runtime tests execute native programs directly. Cross-compiled programs run
only through `CMAKE_CROSSCOMPILING_EMULATOR`, including its argument list. Without
an emulator, configuration, compilation and linking are still checked, while
runtime checks are reported as skipped. A skipped run is not execution evidence,
and earlier build failures remain failures.

Windows ASan fixtures cover configuration selection, configless records, failed
lookup and retry behavior, external SDK resolution, and LLD versus `link.exe`
final-link selection. A separate fixture enables its C++ final-link language
only after embedding the sanitized C library.

Assertion regressions require a test-process startup marker, the expected
assertion diagnostic and the signal handler's dedicated exit status. Unrelated
failures, startup errors and timeouts do not demonstrate a working assertion.
OpenMP package regressions also check that runtime discovery preserves unrelated
parent cache entries and cannot select them as libraries. Focused source tests
cover nullable binary-reader counts, partial matrix-allocation cleanup, moving
regular-expression reallocations, failed backtracking-stack pushes and failed
constrained-state node-set copies, the iterative quicksort sentinel and
transactional allocation bookkeeping, including marker-rejected reallocations,
frees and mcore cleanup.

```sh
cmake -S . -B build/developer -G Ninja -DCMAKE_BUILD_TYPE=Release -DGKLIB_BUILD_DEVELOPER_TESTING=ON -DGKLIB_BUILD_INTEGRATION_TESTING=ON
cmake --build build/developer --parallel 2
ctest --test-dir build/developer --output-on-failure
python tools/upstream.py check
```

Use `upstream/files.json` and the read-only maintenance tool to review official
upstream updates, including identity mappings and intentionally removed files:

```sh
python tools/upstream.py check
python tools/upstream.py compare --to <upstream-commit>
python tools/upstream.py prepare --to <upstream-commit> --output build/upstream-review
```

Replace `<upstream-commit>` with an available commit ID. Candidates and conflict
reports do not modify the working tree, index, or accepted baseline. See
[upstream tracking](upstream.md) and [development checks](development.md).

Integration tests cover installed OpenMP and PCRE consumption, parent-provided
PCRE linkage metadata, template ownership, assertion policies and static
headers. Additional bundled-uninstall and UNIX DESTDIR checks are registered
when a METIS parent checkout is available; this optional coverage is not a
requirement for standalone GKlib. All fixtures use binary directories. Use
`cmake --build build/release --target clean` to clean build output. CTest memory
checking requires an external configured checker.
