#-------------------------------------------------------------------------------
# API AND PROGRAM TESTS
#-------------------------------------------------------------------------------
include("${CMAKE_CURRENT_SOURCE_DIR}/TestSupport.cmake")
set(_gklib_test_initial_cache
  "${CMAKE_CURRENT_BINARY_DIR}/test-initial-cache.cmake")
gklib_test_write_initial_cache("${_gklib_test_initial_cache}")

add_executable(gklib-api-test api.c)
target_link_libraries(gklib-api-test PRIVATE GKlib::GKlib)
gklib_test_add_runtime(NAME GKlib.api TARGET gklib-api-test
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# The standalone fixtures instantiate the exact allocation template, tracker
# sources and bundled regex so controlled allocators can exercise ownership.
add_executable(gklib-allocation-matrix-test allocation_matrix_failure.c)

add_executable(gklib-memory-failure-recovery-test memory_failure_recovery.c)
target_compile_definitions(gklib-memory-failure-recovery-test PRIVATE
  GKLIB_STATIC_DEFINE)
target_include_directories(gklib-memory-failure-recovery-test PRIVATE
  "${PROJECT_SOURCE_DIR}/include"
  "${GKLIB_GENERATED_INCLUDE_DIR}")
set_property(SOURCE memory_failure_recovery.c PROPERTY OBJECT_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/mcore.c"
  "${PROJECT_SOURCE_DIR}/src/memory.c"
  "${PROJECT_SOURCE_DIR}/src/memory_internal.h")

add_executable(gklib-regex-realloc-test regex_realloc_failure.c)
target_compile_definitions(gklib-regex-realloc-test PRIVATE GKLIB_STATIC_DEFINE)
target_include_directories(gklib-regex-realloc-test PRIVATE
  "${PROJECT_SOURCE_DIR}/include"
  "${GKLIB_GENERATED_INCLUDE_DIR}")

# The linked sort fixture instantiates the public template used by applications.
# The API test above also verifies accounting through the linked allocator.
add_executable(gklib-sort-sentinel-test sort_sentinel.c)
target_link_libraries(gklib-sort-sentinel-test PRIVATE GKlib::GKlib)

add_executable(gklib-binary-reader-short-read-test binary_reader_short_read.c)
target_link_libraries(gklib-binary-reader-short-read-test PRIVATE GKlib::GKlib)
set_property(SOURCE binary_reader_short_read.c PROPERTY OBJECT_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/io.c")

set_target_properties(
  gklib-allocation-matrix-test
  gklib-binary-reader-short-read-test
  gklib-memory-failure-recovery-test
  gklib-regex-realloc-test
  gklib-sort-sentinel-test
  PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS YES)

foreach(test IN ITEMS
    allocation-matrix
    binary-reader-short-read
    memory-failure-recovery
    regex-realloc
    sort-sentinel)
  gklib_test_add_runtime(NAME "GKlib.${test}"
    TARGET "gklib-${test}-test"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties("GKlib.${test}" PROPERTIES TIMEOUT 30)
endforeach()

# Each binary reader accepts a null optional count while preserving the same
# allocation and data ownership contract as a counted read.
add_executable(gklib-binary-readers-test binary_readers.c)
target_link_libraries(gklib-binary-readers-test PRIVATE GKlib::GKlib)
foreach(type IN ITEMS char int32 int64 ssize float double)
  gklib_test_add_runtime(NAME GKlib.binary-reader.${type}
    TARGET gklib-binary-readers-test
    ARGS ${type} "${CMAKE_CURRENT_BINARY_DIR}/binary-reader-${type}.bin"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(GKlib.binary-reader.${type} PROPERTIES TIMEOUT 30)
endforeach()

if(GKLIB_BUILD_PROGRAMS)
  gklib_test_add_runtime(NAME GKlib.gksort TARGET gksort
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(GKlib.gksort PROPERTIES TIMEOUT 30
    FAIL_REGULAR_EXPRESSION "error at index")
endif()

# The C++ consumer verifies public headers and thread-related link requirements.
find_package(Threads REQUIRED)
add_executable(gklib-cxx-consumer-test cxx_consumer.cpp)
target_compile_features(gklib-cxx-consumer-test PRIVATE cxx_std_11)
target_link_libraries(gklib-cxx-consumer-test PRIVATE GKlib::GKlib Threads::Threads)
gklib_test_add_runtime(NAME GKlib.cxx-consumer TARGET gklib-cxx-consumer-test
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

set_tests_properties(GKlib.api GKlib.cxx-consumer PROPERTIES
  TIMEOUT 30
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# Copy runtime dependencies beside build-tree test executables on Windows.
if(WIN32)
  foreach(test
      gklib-api-test
      gklib-binary-reader-short-read-test
      gklib-binary-readers-test
      gklib-cxx-consumer-test
      gklib-sort-sentinel-test)
    set(runtime_dlls "$<TARGET_RUNTIME_DLLS:${test}>")
    if(TARGET GKlib::ASanRuntime)
      string(APPEND runtime_dlls
        ";$<GENEX_EVAL:$<TARGET_PROPERTY:GKlib::ASanRuntime,_ASAN_DLLS>>")
    endif()
    if(_gklib_native_asan_runtime_dll)
      string(APPEND runtime_dlls ";${_gklib_native_asan_runtime_dll}")
    endif()
    add_custom_command(TARGET ${test} POST_BUILD
      COMMAND ${CMAKE_COMMAND}
        "-DLIBRARIES=${runtime_dlls}"
        "-DDESTINATION=$<TARGET_FILE_DIR:${test}>"
        -P "${PROJECT_SOURCE_DIR}/cmake/CopyRuntime.cmake"
      VERBATIM)
  endforeach()
endif()

#-------------------------------------------------------------------------------
# INTEGRATION TESTS
#-------------------------------------------------------------------------------
# These tests configure nested producers and consumers, so they remain opt-in.
if(GKLIB_BUILD_INTEGRATION_TESTING)
  if(GKLIB_INSTALL)
    set(_gklib_test_native_msvc_asan FALSE)
    if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "MSVC" AND
        "address" IN_LIST GKLIB_SANITIZERS)
      set(_gklib_test_native_msvc_asan TRUE)
    endif()

    # Windows MSVC-ABI Clang and Intel LLVM targets expose an imported runtime
    # target. Native MSVC instead asks its linker to infer the SDK libraries,
    # so exercise its installed interface without requiring that target.
    if(TARGET GKlib::ASanRuntime OR _gklib_test_native_msvc_asan)
      add_subdirectory(sanitizers)
    endif()
  endif()

  set(_gklib_integration_script
    "${CMAKE_CURRENT_SOURCE_DIR}/integration/run_fixture.cmake")
  set(_gklib_integration_work
    "${CMAKE_CURRENT_BINARY_DIR}/integration")
  set(_gklib_integration_common
    "-DGKLIB_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DMETIS_SOURCE_DIR=${PROJECT_SOURCE_DIR}/../.."
    "-DWORK_ROOT=${_gklib_integration_work}"
    "-DGENERATOR=${CMAKE_GENERATOR}"
    "-DGENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}"
    "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
    "-DTEST_INITIAL_CACHE=${_gklib_test_initial_cache}"
    "-DC_COMPILER=${CMAKE_C_COMPILER}"
    "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCROSSCOMPILING=${CMAKE_CROSSCOMPILING}"
    "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
    "-DCONFIG=$<CONFIG>")

  function(gklib_add_integration_test name scenario)
    add_test(NAME "GKlib.integration.${name}"
      COMMAND "${CMAKE_COMMAND}"
        ${_gklib_integration_common}
        ${ARGN}
        "-DSCENARIO=${scenario}"
        -P "${_gklib_integration_script}")
    set_tests_properties("GKlib.integration.${name}" PROPERTIES
      PROCESSORS 2
      RUN_SERIAL TRUE
      TIMEOUT 300)
    if(CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
      set_tests_properties("GKlib.integration.${name}" PROPERTIES
        SKIP_REGULAR_EXPRESSION
          "^GKLIB_RUNTIME_SKIPPED: no cross-compiling emulator")
    endif()
  endfunction()

  gklib_add_integration_test(openmp-cxx openmp-cxx)
  gklib_add_integration_test(openmp-cache openmp-cache)
  gklib_add_integration_test(subproject subproject)

  # The separately selected MSVC LLVM runtime needs its own development and
  # execution environment. Exercise it when the caller selected that SDK.
  if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" AND
      (OpenMP_RUNTIME_MSVC STREQUAL "llvm" OR
       OpenMP_C_FLAGS MATCHES "[-/]openmp:llvm($|[ ;])"))
    gklib_add_integration_test(openmp-llvm openmp-llvm)
  endif()

  # Reuse the explicitly selected PCRE SDK; do not download a test dependency.
  set(_gklib_pcre_test_args)
  if(GKLIB_REGEX_BACKEND_RESOLVED STREQUAL "PCRE")
    foreach(_pcre_variable IN ITEMS
        PCRE_INCLUDE_DIR PCRE_POSIX_LIBRARY PCRE_LIBRARY)
      list(APPEND _gklib_pcre_test_args
        "-D${_pcre_variable}=${${_pcre_variable}}")
    endforeach()
    list(APPEND _gklib_pcre_test_args
      "-DPARENT_PCRE_LINKAGE=${GKLIB_CONFIG_PCRE_LINKAGE}")
    gklib_add_integration_test(pcre-package pcre-package
      ${_gklib_pcre_test_args})
  endif()

  # Optional language discovery is confined to the integration suite. Normal
  # builds never require Fortran or change the parent's enabled languages.
  include(CheckLanguage)
  check_language(Fortran)
  if(CMAKE_Fortran_COMPILER)
    list(APPEND _gklib_integration_common
      "-DFORTRAN_COMPILER=${CMAKE_Fortran_COMPILER}")
    gklib_add_integration_test(openmp-fortran openmp-fortran)

    if(GKLIB_REGEX_BACKEND_RESOLVED STREQUAL "PCRE")
      gklib_add_integration_test(pcre-fortran pcre-fortran
        ${_gklib_pcre_test_args})
      gklib_add_integration_test(pcre-parent-target pcre-parent-target
        ${_gklib_pcre_test_args})
    endif()
  endif()

  gklib_add_integration_test(assertions assertions)
  gklib_add_integration_test(template-consumer template-consumer)
  gklib_add_integration_test(regex-link regex-link)
  if(WIN32)
    gklib_add_integration_test(static-direct static-direct)
  endif()
  if(EXISTS "${PROJECT_SOURCE_DIR}/../../src/metislib.h")
    gklib_add_integration_test(bundled-uninstall bundled-uninstall)
  endif()
endif()

# Install ownership tests share the opt-in integration-test scope but exercise
# the standalone manifest verifier rather than the scenario fixture.
if(GKLIB_BUILD_INTEGRATION_TESTING)
  # Verify the toolchain-cache serialization and runtime execution boundary
  # used by nested integration and cross-compilation fixtures.
  add_test(NAME gklib.test-support
    COMMAND "${CMAKE_COMMAND}"
      "-DMODULE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/TestSupport.cmake"
      -DFUNCTION_PREFIX=gklib
      "-DSKIP_MARKER=GKLIB_RUNTIME_SKIPPED: no cross-compiling emulator"
      "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/test-support"
      "-DGENERATOR=${CMAKE_GENERATOR}"
      "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
      "-DGENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}"
      "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/test-support/verify.cmake")
  set_tests_properties(gklib.test-support PROPERTIES TIMEOUT 120)

  add_test(NAME gklib.install-ownership
    COMMAND "${CMAKE_COMMAND}"
      "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}" "-DTEST_PROJECT=GKlib"
      "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/install-ownership"
      "-DGENERATOR=${CMAKE_GENERATOR}"
      "-DGENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}"
      "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
      "-DTEST_INITIAL_CACHE=${_gklib_test_initial_cache}"
      "-DTEST_CONFIG=$<CONFIG>"
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
      "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/install/verify.cmake")
  set_tests_properties(gklib.install-ownership PROPERTIES TIMEOUT 300)
  add_test(NAME gklib.install-ownership-shared
    COMMAND "${CMAKE_COMMAND}"
      "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}" "-DTEST_PROJECT=GKlib"
      "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/shared/install-ownership"
      -DTEST_SHARED=ON
      "-DGENERATOR=${CMAKE_GENERATOR}"
      "-DGENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}"
      "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
      "-DTEST_INITIAL_CACHE=${_gklib_test_initial_cache}"
      "-DTEST_CONFIG=$<CONFIG>"
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
      "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/install/verify.cmake")
  set_tests_properties(gklib.install-ownership-shared PROPERTIES TIMEOUT 300)
endif()

# Exercise IPO policy and cold/hot capability caches in an isolated build tree.
if(GKLIB_BUILD_INTEGRATION_TESTING)
  add_test(NAME gklib.ipo-policy-cache
    COMMAND "${CMAKE_COMMAND}"
      "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/ipo-policy-cache"
      "-DGENERATOR=${CMAKE_GENERATOR}"
      "-DGENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}"
      "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
      "-DTEST_INITIAL_CACHE=${_gklib_test_initial_cache}"
      "-DTEST_CONFIG=$<CONFIG>"
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
      "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/ipo/verify.cmake")
  set_tests_properties(gklib.ipo-policy-cache PROPERTIES TIMEOUT 300)
endif()

# Reject static CRT state at a GKlib DLL boundary using configure-only probes.
if(GKLIB_BUILD_INTEGRATION_TESTING AND WIN32 AND
    (MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
  add_test(NAME gklib.shared-crt-policy
    COMMAND "${CMAKE_COMMAND}"
      "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/shared-crt-policy"
      "-DGENERATOR=${CMAKE_GENERATOR}"
      "-DGENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}"
      "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
      "-DTEST_INITIAL_CACHE=${_gklib_test_initial_cache}"
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
      "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
      "-DMODULE_FILE=${PROJECT_SOURCE_DIR}/cmake/GKlibCRT.cmake"
      -DCHECK_FUNCTION=gklib_check_shared_crt
      -DFIXTURE_PROJECT=GKLIB
      -P "${CMAKE_CURRENT_SOURCE_DIR}/crt/verify.cmake")
  set_tests_properties(gklib.shared-crt-policy PROPERTIES TIMEOUT 300)
endif()


# Maintenance regressions exercise merge behavior, runtime SDK discovery, and
# bounded recovery when Windows temporarily locks a runtime copy destination.
# Repository prose and formatting are reviewed separately from CTest.
if(GKLIB_BUILD_DEVELOPER_TESTING)
  find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)

  add_test(NAME gklib.upstream-tool
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
      -s "${CMAKE_CURRENT_SOURCE_DIR}/upstream" -p "test*.py")
  add_test(NAME gklib.intel-runtime
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
      -s "${CMAKE_CURRENT_SOURCE_DIR}/intelruntime" -p "test*.py")
  add_test(NAME gklib.copy-runtime
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
      -s "${CMAKE_CURRENT_SOURCE_DIR}/copyruntime" -p "test*.py")
  set_tests_properties(gklib.upstream-tool gklib.intel-runtime
    gklib.copy-runtime PROPERTIES
    ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1;CMAKE_COMMAND=${CMAKE_COMMAND};COPY_RUNTIME_MODULE=${PROJECT_SOURCE_DIR}/cmake/CopyRuntime.cmake"
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}" TIMEOUT 120)
endif()
