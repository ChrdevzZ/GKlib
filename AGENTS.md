# Repository guidance: GKlib

## Scope and sources

This is the shared guidance for coding agents working on ChrdevzZ/GKlib, either
standalone or as a dependency. Read [README](README.md), [building](docs/building.md)
and [fork changes](FORK_CHANGES.md). Official KarypisLab provenance and accepted
blobs are recorded in `upstream/files.json`; the modification destination is the
ChrdevzZ fork. This file does not require or import a parent METIS checkout.

When embedded, GKlib-specific commands and contracts here apply to its subtree;
retain applicable parent integration constraints. Keep GKlib independently
configurable, buildable, installable and consumable through `GKlib::GKlib`.

## Project map

GKlib is a C utility library for data structures, sorting, memory management,
I/O, strings and numerical operations. Preserve the existing public interfaces
and third-party licenses; see `LICENSES.md` as well as `LICENSE.txt`.

- `include/GKlib.h`: public umbrella header, including generated config/export
  headers. Public templates and their transitive types belong in `include/`.
- `include/gk_mk*.h`: type-generic macro implementations; `gk_proto.h` declares
  their library-owned instances. Original PROTO forms remain caller-owned;
  explicit-export PROTO_EX forms specify the owning library's attributes.
- `src/`: implementations, including CSR/graph routines, template instantiations,
  allocation tracking and error handling; `apps/`: eleven utility programs.
- Root `CMakeLists.txt`: project options and platform/regex checks.
- `cmake/`: reusable target/probe helpers and packaging;
  `tests/`: real API/C++ and optional integration/developer tests.

Use [architecture notes](docs/architecture.md) for data structures and conventions.
Preserve configured TLS and exported data/accessor behavior. Use dedicated GKlib
assertion options/macros instead of propagating NDEBUG to unrelated consumers.

## Build and test

Use CMake 3.24 or newer and a compiler capable of compiling these C99 sources.
Tests additionally need C++11. Ninja examples require Ninja and an initialized
compiler environment. Handwritten make config entry points no longer exist.

```sh
cmake -S . -B build/agent -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/agent --parallel 2
ctest --test-dir build/agent --output-on-failure
```

For multi-config generators omit CMAKE_BUILD_TYPE and pass `--config Release`
to build/install and `-C Release` to CTest. The GKlib default preset is Debug;
release, portable and optimized select Release. Programs/tests/install default
off in subdirectory builds; the option is `GKLIB_BUILD_PROGRAMS`, not BUILD_APPS.

Enable `GKLIB_BUILD_TESTING`, `GKLIB_BUILD_INTEGRATION_TESTING` and
`GKLIB_BUILD_DEVELOPER_TESTING` for top-level developer coverage; integration requires
OpenMP and developer checks require Python/Git. METIS-dependent integration
fixtures are optional when that repository is absent. Shared GKlib on Windows
requires `/MD[d]` because FILE and signal state cross the DLL boundary; static
GKlib supports compatible `/MT[d]` consumers. Intel static archives may need
external Intel runtimes even with IPO off. See the build reference for details.

For embedded full-suite runs, also enable `GKLIB_BUILD_PROGRAMS` so
program-dependent tests are registered; test switches alone do not build tools.

## Editing and build contracts

- Preserve existing user changes, public APIs, upstream attribution and licenses.
  Keep production C/header changes narrowly justified; do not bulk-format them.
- Follow neighboring C declarations, indentation, comments and macro continuation
  style. Use two-space CMake indentation, a blank line between responsibilities
  and two between independent functions or large sections. Do not split generator
  expressions or rewrite embedded source strings. Use English comments that
  explain contracts, not line-by-line translations. Write edited Markdown as LF.
- Keep source lists explicit and properties target-local. Do not introduce global
  compiler/CRT/optimization policy, generated files in source directories, or
  automatic network access. Keep standalone and add_subdirectory builds usable.
- Preserve caller-owned template export attributes and library-owned allocation
  boundaries. Check architecture, widths, CRT and external runtimes separately;
  compiler brand alone neither proves nor disproves C ABI compatibility. IPO
  archives require additional final-linker compatibility.
- Keep local tool paths, inventories, logs and one-off scripts in ignored build
  directories. Do not publish those artifacts or infer success from old logs.
  Do not add CI, release actions or dependency updates as an incidental change.

## Validation and reporting

- Run focused tests for the actual change, using separate build/install paths
  for incompatible compilers, CRTs or widths. Default local build parallelism to
  2; do not impose that limit on downstream CMake users.
- For library behavior changes run API/CLI tests; for public headers and package
  interfaces also test installed C/C++ consumers. For build-policy changes use
  the relevant integration fixtures. When final-link interfaces change, test
  downstream C++/Fortran consumers when those compilers are available; GKlib
  itself has no Fortran wrappers.
- Audit mappings with the maintenance tool and review changed source style
  as described in [development](docs/development.md). Python/Git are maintenance
  dependencies only;
  ordinary builds must not require them. Documentation-only changes need link,
  option and example checks rather than a complete compiler matrix.
- Cross compilation may compile/link probes but must not run target programs
  without an appropriate runtime or emulator. Distinguish passed, failed,
  unavailable and unexecuted cases. Report commands, meaningful results and
  limitations without turning local observations into platform guarantees.
- Review source and index changes before finishing; do not stage, commit, push,
  change accepted upstream baselines or update dependency revisions unless the
  task authorizes those actions. Explicit task instructions govern the scope.

## Code Review Rules

Flag regressions in ABI width/export/TLS/CRT ownership, static dependency closure,
parent-project isolation, offline configuration, install/uninstall ownership and
source-tree cleanliness. Require evidence appropriate to the changed behavior.
Check that fork-specific corrections are distinguished from inherited upstream
work, and that generated headers are not mistaken for upstream source files.
