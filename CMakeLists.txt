cmake_minimum_required(VERSION 3.24)

#-------------------------------------------------------------------------------
# PROJECT SETUP
#-------------------------------------------------------------------------------
project(GKlib
  VERSION 0.0.1
  DESCRIPTION "Utility library used by METIS and related projects"
  LANGUAGES C)


# Options derive top-level-only defaults before platform checks resolve the
# configuration values shared by the library and its consumers.
include(GNUInstallDirs)


#-------------------------------------------------------------------------------
# PROJECT OPTIONS AND DEFAULTS
#-------------------------------------------------------------------------------
# Standalone builds expose programs, tests, and install rules by default;
# embedded builds leave those responsibilities with the parent project.
set(_gklib_top_level_default OFF)
if(PROJECT_IS_TOP_LEVEL)
  set(_gklib_top_level_default ON)
endif()

# Follow the conventional shared-library default when a parent defines it.
if(DEFINED BUILD_SHARED_LIBS)
  set(_gklib_shared_default "${BUILD_SHARED_LIBS}")
else()
  set(_gklib_shared_default OFF)
endif()

# x86 implementation paths are enabled only for recognized x86 processors.
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _gklib_processor)
if(_gklib_processor MATCHES "^(x86_64|amd64|x64|i[3-6]86)$")
  set(_gklib_no_x86_default OFF)
else()
  set(_gklib_no_x86_default ON)
endif()

# Select one library type per build; inherit BUILD_SHARED_LIBS when set.
# Static libraries are the default; external runtimes may still be required.
option(GKLIB_BUILD_SHARED_LIBS
  "Build GKlib as a shared library" ${_gklib_shared_default})

# Build the command-line applications. Standalone builds enable them;
# subdirectory builds leave executable selection to the parent project.
option(GKLIB_BUILD_PROGRAMS
  "Build GKlib command-line programs" ${_gklib_top_level_default})

# Register API and consumer tests with CTest. Standalone builds enable
# them by default; embedded builds opt in without changing parent test policy.
option(GKLIB_BUILD_TESTING
  "Build GKlib tests" ${_gklib_top_level_default})

# Generate install and package-export rules. Default to the standalone
# project setting so embedding does not unexpectedly install this project.
option(GKLIB_INSTALL "Generate GKlib install rules" ${_gklib_top_level_default})

# Enable costly internal consistency checks in addition to ordinary
# assertions. Keep OFF for normal builds because these checks add work.
option(GKLIB_ASSERTIONS_EXPENSIVE "Enable expensive assertions" OFF)

# Tune generated code for the build CPU only when explicitly requested.
# Keep OFF for portable binaries; cross and multi-architecture builds reject it.
option(GKLIB_NATIVE_OPTIMIZATION "Optimize for the build machine CPU" OFF)

# Enable OpenMP for supported parallel implementation paths. OFF needs no
# OpenMP runtime; ON requires a usable C OpenMP package and link interface.
option(GKLIB_OPENMP "Enable OpenMP support" OFF)

# Request gprof instrumentation after checking compiler and linker support.
# Keep OFF for production binaries and fail if explicitly requested but unusable.
option(GKLIB_GPROF "Enable gprof instrumentation" OFF)

# Use the bundled alternative random-number generator instead of the default
# backend. Keep OFF to preserve the default reproducible algorithm path.
option(GKLIB_USE_GKRAND "Enable the alternative GKlib random generator" OFF)

# Disable architecture-specific x86 code. Default ON for unrecognized or
# non-x86 targets; callers can explicitly restrict architecture-specific paths.
option(GKLIB_NO_X86
  "Disable x86-specific implementation paths" ${_gklib_no_x86_default})

# Keep error-recovery and allocation-tracking state per thread when supported.
# Disabling TLS makes both states process-global and changes concurrency safety.
option(GKLIB_THREAD_LOCAL_STORAGE
  "Use compiler thread-local storage for error and allocation state" ON)

# Treat warnings from this project as errors without changing parent targets.
# Keep OFF for consumers using compilers with different warning diagnostics.
option(GKLIB_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# AUTO enables ordinary assertions in Debug and disables them otherwise.
# ON/OFF override this project only, without exporting NDEBUG to consumers.
set(GKLIB_ASSERTIONS "AUTO" CACHE STRING "Assertion policy: AUTO, ON, or OFF")
set_property(CACHE GKLIB_ASSERTIONS PROPERTY STRINGS AUTO ON OFF)

# AUTO enables internal debug instrumentation in Debug configurations.
# ON/OFF provide an explicit policy for this project across configurations.
set(GKLIB_DEBUG "AUTO" CACHE STRING "Debug instrumentation policy: AUTO, ON, or OFF")
set_property(CACHE GKLIB_DEBUG PROPERTY STRINGS AUTO ON OFF)

# AUTO inherits explicit parent IPO policy; otherwise static targets stay
# portable and optimized shared configurations probe IPO support. ON requires
# support; OFF disables IPO on this project without changing parent targets.
set(GKLIB_IPO "AUTO" CACHE STRING "IPO policy: AUTO, ON, or OFF")
set_property(CACHE GKLIB_IPO PROPERTY STRINGS AUTO ON OFF)

# AUTO prefers linkable system POSIX regex and falls back to bundled code.
# SYSTEM and PCRE require their dependencies; GKREGEX selects bundled regex.
set(GKLIB_REGEX_BACKEND "AUTO" CACHE STRING
  "Regular expression backend: AUTO, SYSTEM, GKREGEX, or PCRE")
set_property(CACHE GKLIB_REGEX_BACKEND PROPERTY STRINGS AUTO SYSTEM GKREGEX PCRE)

# Request a semicolon-separated list of supported sanitizers. Empty means
# none; explicit requests require usable compile and final-link runtimes.
set(GKLIB_SANITIZERS "" CACHE STRING
  "Semicolon-separated sanitizers, for example address;undefined")

# Normalize cache values once so all downstream comparisons are stable.
foreach(_gklib_tristate IN ITEMS GKLIB_ASSERTIONS GKLIB_DEBUG GKLIB_IPO)
  string(TOUPPER "${${_gklib_tristate}}" ${_gklib_tristate})
  if(NOT ${_gklib_tristate} MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR "${_gklib_tristate} must be AUTO, ON, or OFF")
  endif()
endforeach()

string(TOUPPER "${GKLIB_REGEX_BACKEND}" GKLIB_REGEX_BACKEND)
if(NOT GKLIB_REGEX_BACKEND MATCHES "^(AUTO|SYSTEM|GKREGEX|PCRE)$")
  message(FATAL_ERROR "GKLIB_REGEX_BACKEND must be AUTO, SYSTEM, GKREGEX, or PCRE")
endif()

# Add Python maintenance-tool regressions when GKLIB_BUILD_TESTING is enabled.
# Keep OFF so ordinary library builds need neither Python nor Git.
option(GKLIB_BUILD_DEVELOPER_TESTING
  "Run Python maintenance tool regressions" OFF)

# Add nested configure/build/install consumer tests when GKLIB_BUILD_TESTING is
# enabled. Keep OFF for fast ordinary builds; fixtures own their binary trees.
option(GKLIB_BUILD_INTEGRATION_TESTING
  "Run GKlib package, policy, and install integration regressions" OFF)


# Native MSVC keeps its ASan runtime beside the selected target compiler.
# Record an unambiguous existing file for build-tree programs and tests only;
# the path is neither installed nor exported as a package requirement.
set(_gklib_native_asan_runtime_dll "")
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "MSVC" AND
    "address" IN_LIST GKLIB_SANITIZERS)
  get_filename_component(_gklib_compiler_directory
    "${CMAKE_C_COMPILER}" DIRECTORY)
  file(GLOB _gklib_native_asan_runtime_candidates LIST_DIRECTORIES FALSE
    "${_gklib_compiler_directory}/clang_rt.asan_dynamic-*.dll")
  list(LENGTH _gklib_native_asan_runtime_candidates
    _gklib_native_asan_runtime_count)
  if(_gklib_native_asan_runtime_count EQUAL 1)
    list(GET _gklib_native_asan_runtime_candidates 0
      _gklib_native_asan_runtime_dll)
  endif()
endif()


#-------------------------------------------------------------------------------
# PLATFORM CAPABILITIES
#-------------------------------------------------------------------------------
# Compile/link probes describe the target platform without executing target
# programs. Reusable probe helpers isolate check state from parent projects.
include(CheckCCompilerFlag)
include(CheckCSourceCompiles)
include(CheckIncludeFile)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GKlibCheckLink.cmake")

#-------------------------------------------------------------------------------
# PLATFORM API CHECKS
#-------------------------------------------------------------------------------
# execinfo may be provided by libc or a separate library. Disable the feature
# when the header is present but neither link path supplies backtrace().
check_include_file(execinfo.h HAVE_EXECINFO_H)
set(GKLIB_EXECINFO_LIBRARY "")
if(HAVE_EXECINFO_H)
  set(_gklib_backtrace_source "#include <execinfo.h>
    int main(void) {
      void *frames[1];
      return backtrace(frames, 1) < 0;
    }")
  gklib_check_link("${_gklib_backtrace_source}" GKLIB_BACKTRACE_IN_LIBC)
  if(NOT GKLIB_BACKTRACE_IN_LIBC)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LIBRARIES execinfo)
    gklib_check_link("${_gklib_backtrace_source}"
      GKLIB_BACKTRACE_IN_LIBEXECINFO)
    cmake_pop_check_state()
    if(GKLIB_BACKTRACE_IN_LIBEXECINFO)
      set(GKLIB_EXECINFO_LIBRARY execinfo)
    else()
      set(HAVE_EXECINFO_H FALSE)
    endif()
  endif()
  unset(_gklib_backtrace_source)
endif()

# getline() needs the POSIX feature-test definition used by the library target.
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_DEFINITIONS -D_POSIX_C_SOURCE=200809L)
gklib_check_link("#include <stdio.h>
  int main(void) {
    char *line = 0;
    size_t size = 0;
    return getline(&line, &size, stdin) < 0;
  }" HAVE_GETLINE)
cmake_pop_check_state()
check_include_file(regex.h HAVE_REGEX_H)

#-------------------------------------------------------------------------------
# THREAD-LOCAL LIBRARY STATE
#-------------------------------------------------------------------------------
set(GKLIB_THREAD_LOCAL "")
if(GKLIB_THREAD_LOCAL_STORAGE)
  if(MSVC)
    set(_gklib_tls_candidate "__declspec(thread)")
  else()
    set(_gklib_tls_candidate "__thread")
  endif()
  check_c_source_compiles(
    "${_gklib_tls_candidate} int value; int main(void) { return value; }"
    GKLIB_HAVE_THREAD_LOCAL)
  if(NOT GKLIB_HAVE_THREAD_LOCAL)
    message(FATAL_ERROR
      "GKLIB_THREAD_LOCAL_STORAGE=ON, but the compiler has no supported TLS keyword; configure with GKLIB_THREAD_LOCAL_STORAGE=OFF to use process-global error and allocation state")
  endif()
  set(GKLIB_THREAD_LOCAL "${_gklib_tls_candidate}")
endif()

# Publish source-configuration values consumed by gklib_config.h.
set(NO_X86 "${GKLIB_NO_X86}")
set(USE_GKRAND "${GKLIB_USE_GKRAND}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(LINUX ON)
endif()
set(GKLIB_CONFIG_NEEDS_OPENMP OFF)
set(GKLIB_CONFIG_NEEDS_PCRE OFF)

#-------------------------------------------------------------------------------
# MATH LIBRARY CHECK
#-------------------------------------------------------------------------------
# Prefer libc and expose libm only when the final executable link requires it.
include(CMakePushCheckState)
cmake_push_check_state(RESET)
set(_math_source "#include <math.h>
  int main(void) {
    double (*volatile operation)(double) = sqrt;
    return operation(4.0) != 2.0;
  }")
gklib_check_link("${_math_source}" GKLIB_MATH_IN_LIBC)
set(GKLIB_MATH_LIBRARY "")
if(NOT GKLIB_MATH_IN_LIBC)
  set(CMAKE_REQUIRED_LIBRARIES m)
  gklib_check_link("${_math_source}" GKLIB_MATH_IN_LIBM)
  if(NOT GKLIB_MATH_IN_LIBM)
    message(FATAL_ERROR "A linkable C math library is required")
  endif()
  set(GKLIB_MATH_LIBRARY m)
endif()
cmake_pop_check_state()
unset(_math_source)


# Optional regex libraries belong with the platform capability checks.
# Keep package search hints local while exposing configuration-header values.
function(gklib_check_regex)
  # Publish conservative defaults before resolving the selected regex backend.
  set(USE_PCRE OFF PARENT_SCOPE)
  set(USE_GKREGEX OFF PARENT_SCOPE)
  set(HAVE_PCREPOSIX_H OFF PARENT_SCOPE)

  #-------------------------------------------------------------------------------
  # SYSTEM REGEX PROBE
  #-------------------------------------------------------------------------------
  # A parent may use static try-compiles; regex requires an executable link.
  # The helper also keys its cache on the current required flags and libraries.
  if(HAVE_REGEX_H)
    gklib_check_link("#include <regex.h>
    int main(void) {
      regex_t expression;
      int result = regcomp(&expression, \"x\", REG_EXTENDED);
      if (!result) {
        result = regexec(&expression, \"x\", 0, 0, 0);
        regfree(&expression);
      }
      return result;
    }" GKLIB_HAVE_SYSTEM_REGEX)
  endif()

  #-------------------------------------------------------------------------------
  # REGEX BACKEND SELECTION
  #-------------------------------------------------------------------------------
  # AUTO prefers the platform implementation and falls back to bundled GKREGEX.
  if(GKLIB_REGEX_BACKEND STREQUAL "AUTO")
    if(GKLIB_HAVE_SYSTEM_REGEX)
      set(_gklib_regex_backend SYSTEM)
    else()
      set(_gklib_regex_backend GKREGEX)
    endif()
  else()
    set(_gklib_regex_backend "${GKLIB_REGEX_BACKEND}")
  endif()

  # Validate explicit selections and publish dependency requirements to the
  # directory that creates the GKlib target.
  if(_gklib_regex_backend STREQUAL "SYSTEM")
    if(NOT GKLIB_HAVE_SYSTEM_REGEX)
      message(FATAL_ERROR
        "GKLIB_REGEX_BACKEND=SYSTEM requires regex.h and linkable POSIX regex functions")
    endif()
  elseif(_gklib_regex_backend STREQUAL "GKREGEX")
    set(USE_GKREGEX ON PARENT_SCOPE)
  elseif(_gklib_regex_backend STREQUAL "PCRE")
    list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
    find_package(PCRE REQUIRED)
    if(GKLIB_INSTALL AND NOT PCRE_LINKAGE MATCHES "^(STATIC|SHARED)$")
      message(FATAL_ERROR
        "GKLIB_INSTALL=ON with PCRE::POSIX requires PCRE_LINKAGE=STATIC or "
        "SHARED when linkage cannot be inferred from a concrete PCRE::POSIX "
        "or PCRE::pcreposix target")
    endif()
    set(GKLIB_REGEX_LINK_TARGET PCRE::POSIX PARENT_SCOPE)
    set(USE_PCRE ON PARENT_SCOPE)
    set(HAVE_PCREPOSIX_H ON PARENT_SCOPE)
    set(GKLIB_CONFIG_NEEDS_PCRE ON PARENT_SCOPE)
    set(GKLIB_CONFIG_PCRE_LINKAGE "${PCRE_LINKAGE}" PARENT_SCOPE)
  endif()

  # Keep the resolved choice available for diagnostics and dependent directories.
  set(GKLIB_REGEX_BACKEND_RESOLVED "${_gklib_regex_backend}" CACHE INTERNAL "")
endfunction()

gklib_check_regex()


# Intel C objects may require Intel math/vector runtimes even without IPO.
# Keep discovery local to the build; exported packages rediscover their SDK.
set(GKLIB_CONFIG_NEEDS_INTEL_RUNTIME OFF)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "IntelLLVM")
  function(_gklib_find_intel_runtime)
    list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
    set(IntelRuntime_LIBRARY_DIRS ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES})
    get_filename_component(compiler_directory "${CMAKE_C_COMPILER}" DIRECTORY)
    list(APPEND IntelRuntime_LIBRARY_DIRS "${compiler_directory}/../lib")
    find_package(IntelRuntime REQUIRED MODULE)
  endfunction()
  _gklib_find_intel_runtime()
  if(NOT GKLIB_BUILD_SHARED_LIBS)
    set(GKLIB_CONFIG_NEEDS_INTEL_RUNTIME ON)
  endif()
endif()

#-------------------------------------------------------------------------------
# TARGETS
#-------------------------------------------------------------------------------
# Record the files installed by GKlib targets, headers and package rules.
if(GKLIB_INSTALL)
  include(cmake/InstallManifest.cmake)
  gklib_install_manifest_begin()
endif()

# Generate the public headers before targets consume their build-tree paths.
add_subdirectory(include)

# The implementation directory owns source lists and target-local properties.
add_subdirectory(src)

# Programs consume the configured library target and its public interface.
if(GKLIB_BUILD_PROGRAMS)
  add_subdirectory(apps)
endif()

# C++ is needed only by consumer tests, never by a library-only build.
if(GKLIB_BUILD_TESTING)
  if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
  endif()
  enable_testing()
  enable_language(CXX)
  add_subdirectory(tests)
endif()

#-------------------------------------------------------------------------------
# INSTALLATION AND PACKAGING
#-------------------------------------------------------------------------------
if(GKLIB_INSTALL)
  include(cmake/GKlibInstall.cmake)
  gklib_install_manifest_end()
endif()

# Packaging belongs to standalone builds; parent projects retain ownership of
# their package targets and source archives.
if(PROJECT_IS_TOP_LEVEL)
  include(cmake/GKlibPackage.cmake)
endif()
