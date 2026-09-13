# Changes in the ChrdevzZ GKlib fork

## Source and comparison baseline

| Item | Source |
| --- | --- |
| Fork | [ChrdevzZ/GKlib](https://github.com/ChrdevzZ/GKlib) |
| Official upstream | [KarypisLab/GKlib](https://github.com/KarypisLab/GKlib) |
| Upstream source branch | `master` |
| Accepted upstream commit | [`3b7d61b9f885063c89901f3901fb4426f9cfb58f`](https://github.com/KarypisLab/GKlib/commit/3b7d61b9f885063c89901f3901fb4426f9cfb58f) |
| Fork status | Unreleased changes in the current checkout |

The full SHA, also recorded in [upstream/files.json](upstream/files.json), fixes
the comparison. The branch name describes provenance, not an automatically
updated baseline. This repository is the independently usable GKlib fork;
it can also be included as METIS's `ext/GKlib` submodule. Its modifications belong
to ChrdevzZ/GKlib, not to the official KarypisLab repository.

These changes are Unreleased. A parent project must select a compatible fork
commit, and that commit must be published before remote users can fetch it.
This document does not require a METIS parent checkout to be read or used.

## Inherited functionality

GKlib remains the helper-routine and template library used by METIS and related
software. Public inclusion remains `<GKlib.h>`, and existing helper functions
and caller-owned template declarations remain available. The upstream baseline
already has `src/`, `include/` and `apps/`; this fork does not claim their creation.

Existing notices and third-party origins remain intact. See [LICENSE.txt](LICENSE.txt),
[LICENSES.md](LICENSES.md) and [LICENSES/](LICENSES/). There is no invented upstream
release history. The current changes preserve intended library use but are not
a claim of byte-identical source or unrestricted binary compatibility.

## Build and directory changes

The legacy handwritten Make wrapper and older global CMake configuration are
replaced by CMake 3.24 with target-level options and explicit source lists.
The existing `GKlib` target and public `GKlib::GKlib` alias are retained. Root CMake
coordinates options, platform checks and directories; individual directories
own library, public-header, program, dependency and test responsibilities.
Generated configuration/export headers remain in the binary tree.

Unlike the old global flag approach, project options do not impose optimization,
assertion or CRT settings on unrelated parent targets. Standalone builds enable
tools/tests/install rules; subdirectory builds leave them to the parent. The
obsolete TLS probe and pre-modern-MSVC integer compatibility headers are removed;
standard integer headers are now required. CMake-generated Makefiles are still
supported. See the [build reference](docs/building.md) for the current interface.

## Configuration, optimization and dependencies

Debug and assertion policies can follow the active configuration. Dedicated
GKlib template assertion macros avoid exporting a global NDEBUG policy. Ordinary
static archives under AUTO do not enable IPO unless a parent requests it;
shared optimized configurations probe support, ON requires it and OFF disables
it. Checks are cached for the relevant configuration/toolchain signature.
The portable/optimized presets select explicit IPO policies without native
CPU tuning. GKlib's default preset is Debug, unlike METIS's Release default.

OpenMP, regex, random-number backend, TLS and architecture fallback options
remain configurable under `GKLIB_` names. Regex AUTO checks the platform backend
and falls back to bundled GKREGEX; explicit SYSTEM/PCRE requests require working
dependencies. Platform probes distinguish compilation from final linking.
The root `CMakeLists.txt` manages optional regex dependencies; GKlib itself does not require METIS.

Static OpenMP packages record the producer runtime library names and recover
those exact link dependencies without enabling C or C++ or adding another
compiler's OpenMP flags. Pure Fortran consumers can therefore load the package
when a compatible producer SDK is discoverable through `OpenMP_ROOT` or
`CMAKE_PREFIX_PATH`. A PCRE-enabled package also preserves the static/shared
linkage established by its producer configuration and uses a non-executed
`BIND(C)` link check in pure Fortran projects rather than guessing linkage. A
shared GKlib keeps OpenMP private and needs only runtime deployment from its
consumers.

For MSVC producers, the recorded OpenMP closure distinguishes the native and
experimental VCOMP runtime from the LLVM selection made through CMake's
`OpenMP_RUNTIME_MSVC=llvm`: Release and Debug packages recover `libomp.lib` and
`libompd.lib`, respectively. The LLVM SDK and runtime DLL remain external and
are never bundled by GKlib.

Windows MSVC-ABI Clang and Intel LLVM AddressSanitizer builds record the
producer-selected runtime library filenames for each available configuration.
Installed static consumers recover those libraries from a compatible compiler
SDK through `CompilerRuntime_ROOT` or `CMAKE_PREFIX_PATH`; packages do not export producer
SDK paths or redistribute compiler runtime libraries and DLLs. Build-tree tools
and tests copy the selected DLLs locally, while installed shared libraries keep
runtime deployment as a consumer responsibility. ASan final links using LLD
disable string tail merging according to the final link language and linker
selection; ordinary `link.exe` links do not receive that LLD-only workaround.
Transient Windows permission or sharing conflicts during build-tree runtime
copying receive a bounded retry; missing inputs and persistent failures remain
fatal.

Parent-provided PCRE targets retain explicit linkage metadata or derive it only
from concrete `PCRE::POSIX`/`PCRE::pcreposix` library types. Installing from an
opaque interface requires the parent to state `PCRE_LINKAGE=STATIC|SHARED`.

## Production source and header changes

These are differences from the fixed upstream baseline, not new changes made
by the documentation refresh. Same-path entries are intentional: most GKlib
files did not move. See the mapping for unchanged files and intentional removals.

| Upstream/local path | Change and user impact |
| --- | --- |
| `include/GKlib.h` | Include generated configuration/export headers, select bundled regex declarations when configured, and use the standard `_OPENMP` feature macro before including the runtime header. |
| `include/gk_arch.h` | Use standard integer headers instead of obsolete MSVC polyfills; retain needed platform compatibility. |
| `include/gk_externs.h`, `src/error.c` | Use configured TLS; Windows shared error-state accessors preserve source access while avoiding direct imported TLS data assumptions. This is a DLL representation change. |
| `include/gk_getopt.h`, `src/getopt.c` | Include the generated export definition directly so the public getopt header remains standalone, and apply DLL import/export attributes to declarations and global definitions. Keep the GNU `W;` long-option extension conditional on a supplied long-option table so ordinary `gk_getopt` remains a safe short-option parser. |
| `include/gk_proto.h` | Export GKlib-owned functions and template instantiations explicitly; remove undeclared-ownership fallback declarations for OpenMP runtime functions that GKlib does not define. |
| `include/gk_macros.h` | Separate normal/expensive assertion policy with GKlib-specific macros and retain termination when an enabled assertion fails. |
| `include/gk_mkblas.h`, `include/gk_mkmemory.h`, `include/gk_mkpqueue.h`, `include/gk_mkpqueue2.h`, `include/gk_mkrandom.h`, `include/gk_mkutils.h` | Add `_PROTO_EX` forms with an explicit export-attribute argument. Original forms remain caller-owned and do not inherit GKlib DLL ownership. Matrix templates also release the outer pointer table after a partial row-allocation failure. |
| `include/gk_mksort.h` | Initialize the iterative quicksort's bottom stack sentinel once before it can be popped; this adds no work to the partition loop. |
| `include/gkregex.h`, `src/gkregex.c` | Apply export/configuration declarations, avoid redefining alloca and use pointer-sized integer casts on LLP64 systems. DFA and multibyte range growth commit each successful moving reallocation before attempting the next array. Register-pair allocation failures reset both pointers to a consistent, retryable state, backtracking-stack entries become visible only after all owned copies succeed, and constrained state construction propagates failed state allocation or owned node-set copies without registering a partial state. Preserve third-party attribution. |
| `src/io.c` | Permit every binary reader's optional element-count output to be null and correct the parameter documentation from lines to elements. Close the input stream before reporting a short read so returning errors and signal recovery do not leak the file handle. |
| `src/mcore.c`, local `src/memory_internal.h` | Reserve operation-stack capacity before allocating payloads or changing core state, and commit moving growth only after it succeeds. Constructors release partial objects on allocation failure. Delete, pop and destroy operations retain their signatures, signal through the configured error path and preserve the current record or caller handle when an active tracker marker rejects the release. |
| `src/memory.c` | Use configured TLS for allocation tracking, reserve bookkeeping capacity before allocating payloads, and preserve the old tracked allocation when reallocation fails. Successful reallocations update their existing slot and cumulative statistics; internal mcore-stack growth can update the owning record across active tracker markers without relaxing the public reallocation boundary. A rejected tracked free preserves that pointer and stops the remaining variadic release list. Also correct the `gk_malloc` comment, release the outer pointer table after partial matrix allocation, and remove accepted bookkeeping before freeing storage. |
| `src/string.c` | Make `gk_strstr_replace` copy sized fragments with `memcpy`, write escapes to the output buffer, advance and terminate global empty matches, mark suffix scans `REG_NOTBOL`, and ignore absent captures safely. |
| `src/timers.c` | Remove a disabled OpenMP timing branch and correct wall-clock/CPU-clock comments; retain the existing platform time sources. |
| Removed `include/gk_ms_stdint.h`, `include/gk_ms_inttypes.h` | Require modern standard integer headers instead of maintaining obsolete compiler definitions. |

`include/gklib_config.h.in` is a local template; CMake generates it and the export
header into the binary tree. Generated headers must accompany installed public
headers. Public templates and their transitive types remain part of the public
interface, not private files merely because their names look internal.

## Installation, compatibility and validation

Relocatable Config/Version/Targets files recover external targets needed by
static consumers, including OpenMP and PCRE when selected. GNUInstallDirs governs
library/header/program placement; Windows DLLs go into the binary directory.
The dedicated uninstall target deletes only recorded GKlib files, preserving
actual postfixes, components, configurations, prefixes and staged destinations
rather than guessing ownership from filenames.

Ordinary C objects may cross compatible compiler families. Architecture, calling
ABI, CRT, public types and ownership still have to match. Intel-produced static
archives may require external Intel runtimes even with IPO off; consumer-side
lookup through `IntelRuntime_ROOT` or `CMAKE_PREFIX_PATH` restores the dependency.
No Intel binaries are bundled or installed. Completed shared libraries have
runtime deployment requirements without forcing consumers to locate static SDK
libraries. IPO archives retain extra toolchain compatibility constraints.

MSYS2 producer builds remain isolated. UCRT64 and CLANG64 share UCRT and can
consume compatible installed C interfaces when the remaining ABI and runtime
contracts match; their libstdc++ and libc++ object interfaces do not cross that
boundary. MINGW64 uses the different MSVCRT runtime.

Shared GKlib requires `/MD[d]`: FILE and signal state cannot safely cross separate
static CRT instances. Static GKlib supports `/MT[d]`. Windows error-state symbols
use accessors for shared builds, so source-level access is not a guarantee of
old raw TLS symbol ABI. See [platform contracts](docs/building.md#platforms-and-mixed-compilers).

Tests cover C/C++ headers, API use, exported template ownership, assertion policy,
package consumption, relocation and installation ownership. Optional integration
fixtures can exercise METIS embedding when it is present. These capabilities are
not evidence that every compiler/platform combination was executed; keep actual
machine results outside published sources.

## Migrating legacy commands

| Legacy entry | Current interface |
| --- | --- |
| `make config`, `make`, `make install` | `cmake -S/-B`, `cmake --build`, `cmake --install` |
| `cc`, `prefix`, `shared` / `SHARED` | `CMAKE_C_COMPILER`, `CMAKE_INSTALL_PREFIX`, `GKLIB_BUILD_SHARED_LIBS` |
| `openmp` / `OPENMP` | `GKLIB_OPENMP` |
| `GDB`, `DEBUG`, `ASSERT`, `ASSERT2` | Standard Debug/RelWithDebInfo, `GKLIB_DEBUG`, `GKLIB_ASSERTIONS`, `GKLIB_ASSERTIONS_EXPENSIVE` |
| `GPROF`, `GKRAND`, `NO_X86` | `GKLIB_GPROF`, `GKLIB_USE_GKRAND`, `GKLIB_NO_X86` |
| `GKREGEX`, `PCRE` | `GKLIB_REGEX_BACKEND` selection |
| `make clean`, `make distclean` | Build the `clean` target; remove only the chosen binary directory for a full reset |
| `make uninstall` | Build `gklib-uninstall` |
| Legacy source packaging | CMake `package_source` target or CPack source configuration |

Old aliases are removed. Complete current defaults live in the
[configuration reference](docs/building.md#build-configuration).

## Maintenance and history

The [file mapping](upstream/files.json) is the machine-readable record of
upstream/local paths, accepted blobs, templates and intentional deletions.
Generated headers are build products, not upstream source baselines. See
[upstream tracking](docs/upstream.md) for read-only comparison and candidate
patch generation. Ordinary builds do not require Python or Git.

When accepting an upstream update, review its source patch and mapping together,
then update the baseline in this document and the mapping. Remove changes from
the current-difference sections when upstream has absorbed them; preserve their
history. A newer upstream branch tip alone does not change the accepted baseline.
Do not automatically restore intentionally removed build files.

### Unreleased

- Isolate OpenMP runtime lookup from parent cache variables and require explicit
  assertion execution evidence in negative tests.
- Preserve toolchain inputs and the selected configuration in nested tests,
  including Fortran-only consumers; distinguish emulator execution from
  compile/link-only cross checks.

- Require a real executable link for system regex detection, even when a parent
  uses static try-compiles; recheck when required flags or libraries change.

- Keep project options and platform checks in the root CMakeLists.txt, with
  per-option explanations; retain reusable helpers and packaging in cmake/.

- Replace Claude-only guidance with canonical AGENTS.md instructions and thin
  Claude/Gemini import adapters; preserve architecture context in developer docs.

- Replace legacy configuration entry points with target-based CMake builds,
  installation packages and explicit developer checks.
- Restore standalone inclusion of `gk_getopt.h`, use the standard OpenMP feature
  macro and stop declaring runtime-owned fallback functions that GKlib does not
  implement.
- Preserve a static producer's OpenMP runtime closure without forcing C/C++
  language discovery in installed-package consumers; shared packages do not
  require the private OpenMP development SDK.
- Record producer-validated PCRE linkage so pure Fortran package consumers can
  verify the C symbols with a link-only `BIND(C)` probe without guessing static
  versus shared behavior.
- Preserve parent-provided PCRE linkage from concrete canonical targets and
  require explicit linkage metadata only when an installing build receives an
  opaque interface graph.
- Add portable static/optimized IPO policies, external Intel runtime discovery,
  and compatible mixed-compiler consumption with explicit CRT boundaries.
- Validate IPO with configuration-specific compile/link projects and reusable
  diagnostics, including real invalid-link and AUTO recovery regressions.
- Make feature link checks use the active configuration, selected linker and
  strict diagnostics for ignored options while preserving parent check state.
- Preserve Windows MSVC-ABI Clang and Intel LLVM ASan runtime closure without
  exporting producer SDK paths or redistributing compiler runtimes, and apply
  the string-tail-merge workaround only to relevant LLD final links.
- Recover from transient Windows runtime-copy sharing conflicts within a bounded
  retry budget while retaining fatal missing-input and persistent-lock errors.
- Repair partial matrix-allocation cleanup, regular-expression DFA, multibyte
  range, register-pair, backtracking-stack and constrained-state allocation and copy
  ownership, quicksort sentinel initialization and allocation bookkeeping.
  Make mcore growth and tracked allocation, reallocation and release
  transactional under the configured allocation-error policy; add deterministic
  regressions for marker-rejected frees, pops and destruction.
- Accept null optional counts in binary readers and test all supported element
  types without changing their allocated-result ownership; force short reads to
  verify stream cleanup for returning errors and signal recovery.
- Keep `gk_getopt` safe when the short-option string contains GNU's `W;`
  extension without a long-option table, while retaining long-option mapping,
  no-match and missing-argument behavior in `gk_getopt_long`.
- Keep allocation accounting in the linked API test and reserve separate
  failure-injection fixtures for recovery and ownership contracts.
- Retain the platform and public-header corrections described above and add
  regression coverage for their build and consumer contracts.
- Separate the introductory README, complete build reference and fixed-baseline
  fork comparison; preserve upstream ownership and license notices.

These entries describe the current modified checkout, not a published fork
release. Add actual release identifiers only after a release exists. Public
documentation records reproducible workflows and limitations, not workstation
inventories, absolute paths or unrepeatable performance claims.
