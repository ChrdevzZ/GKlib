# GKlib

> **Note:** this is the [ChrdevzZ/GKlib](https://github.com/ChrdevzZ/GKlib) fork
> of [KarypisLab/GKlib](https://github.com/KarypisLab/GKlib), with a modernized
> CMake build and integration support. See [fork changes](FORK_CHANGES.md) for
> the fixed upstream baseline, differences, and Unreleased history.

A library of various helper routines and frameworks used by many of the lab's
software.

## Requirements

CMake 3.24 or newer, a C compiler capable of compiling the C99 sources, and
a build tool supported by CMake are required. The Ninja examples below require
Ninja on PATH. Python and Fortran are optional maintenance/consumer tools. A
C++11 compiler is also required when GKlib tests are enabled.

## Download

```sh
git clone https://github.com/ChrdevzZ/GKlib.git
cd GKlib
```

The commands below describe the current checkout; see the Unreleased notes
before treating a remote revision as containing these changes.

## Build and install

```sh
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel 2
ctest --test-dir build/release --output-on-failure
cmake --install build/release --prefix /path/to/prefix
```

These commands build the static library and standalone tools and tests. Select
an initialized compiler environment first. For a multi-config generator, omit
`CMAKE_BUILD_TYPE` and pass `--config Release` to build and install, and
`-C Release` to CTest. See the [build reference](docs/building.md).

Set `GKLIB_BUILD_SHARED_LIBS=ON` for a shared library. The `portable` and
`optimized` presets select conservative or required IPO policies; they do not
change the library type. IPO checks compile and link for the active configuration
without running target programs. See [configuration](docs/building.md#build-configuration)
for all options and preset differences.

## Use in another project

Link an existing application target to an installed package:

```cmake
find_package(GKlib CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE GKlib::GKlib)
```

Set `CMAKE_PREFIX_PATH` to its installation prefix when configuring the parent
project. Alternatively, include a source checkout:

```cmake
add_subdirectory(ext/GKlib)
target_link_libraries(my_application PRIVATE GKlib::GKlib)
```

Include `<GKlib.h>` in application code. Subdirectory builds leave programs,
tests, and installation disabled unless requested by the parent. See
[installation and consumption](docs/building.md#installation-and-consumption)
for dependency, ABI, and static/shared combinations.

Windows consumers must respect CRT ownership, external compiler runtimes for
Intel-built archives, and ASan SDK and deployment requirements for
sanitizer-instrumented libraries. See
[configuration](docs/building.md#build-configuration) and
[platforms and mixed compilers](docs/building.md#platforms-and-mixed-compilers).

## Development

Coding agents should read [AGENTS.md](AGENTS.md); see
[agent setup](docs/agents.md) for client discovery and adapters.

See [development checks](docs/development.md), [upstream tracking](docs/upstream.md),
and [fork changes](FORK_CHANGES.md). Local toolchain paths and validation results
belong in ignored build directories.

## License

GKlib is licensed under the Apache License, Version 2.0. Bundled third-party
files retain their original licenses, making the combined work
`Apache-2.0 AND LGPL-2.1-or-later AND BSD-3-Clause`. See
[LICENSES.md](LICENSES.md) and [LICENSES/](LICENSES/).
