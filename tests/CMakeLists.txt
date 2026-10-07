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
  "${PROJECT_SOURCE_DIR}/src/memory_internal.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/memory_failure_support.h")

add_executable(gklib-pqueue-failure-recovery-test
  pqueue_failure_recovery.c)
target_compile_definitions(gklib-pqueue-failure-recovery-test PRIVATE
  GKLIB_STATIC_DEFINE)
target_include_directories(gklib-pqueue-failure-recovery-test PRIVATE
  "${PROJECT_SOURCE_DIR}/include"
  "${GKLIB_GENERATED_INCLUDE_DIR}")
set_property(SOURCE pqueue_failure_recovery.c PROPERTY OBJECT_DEPENDS
  "${PROJECT_SOURCE_DIR}/include/gk_mkpqueue.h"
  "${PROJECT_SOURCE_DIR}/include/gk_mkpqueue2.h"
  "${PROJECT_SOURCE_DIR}/src/error.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/memory_failure_support.h")
if(NOT WIN32)
  gklib_test_link_threads(gklib-pqueue-failure-recovery-test)
endif()

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
# This fixture includes io.c after GKlib.h, so match the producer's feature
# level before any system header fixes the available getline declarations.
target_compile_definitions(gklib-binary-reader-short-read-test PRIVATE
  "$<$<PLATFORM_ID:Linux>:_POSIX_C_SOURCE=200809L>")
target_link_libraries(gklib-binary-reader-short-read-test PRIVATE GKlib::GKlib)
set_property(SOURCE binary_reader_short_read.c PROPERTY OBJECT_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/io.c")

add_executable(gklib-seq-reader-test seq_reader.c)
target_link_libraries(gklib-seq-reader-test PRIVATE GKlib::GKlib)

add_executable(gklib-cache-size-safety-test cache_size_safety.c)
target_link_libraries(gklib-cache-size-safety-test PRIVATE GKlib::GKlib)

add_executable(gklib-cache-failure-recovery-test cache_failure_recovery.c)
target_compile_definitions(gklib-cache-failure-recovery-test PRIVATE
  GKLIB_STATIC_DEFINE)
target_include_directories(gklib-cache-failure-recovery-test PRIVATE
  "${PROJECT_SOURCE_DIR}/include"
  "${GKLIB_GENERATED_INCLUDE_DIR}")
set_property(SOURCE cache_failure_recovery.c PROPERTY OBJECT_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/cache.c"
  "${PROJECT_SOURCE_DIR}/src/memory_internal.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/memory_failure_support.h")

add_executable(gklib-htable-safety-test htable_safety.c)
target_compile_definitions(gklib-htable-safety-test PRIVATE
  GKLIB_STATIC_DEFINE)
target_include_directories(gklib-htable-safety-test PRIVATE
  "${PROJECT_SOURCE_DIR}/include"
  "${GKLIB_GENERATED_INCLUDE_DIR}")
set_property(SOURCE htable_safety.c PROPERTY OBJECT_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/htable.c"
  "${PROJECT_SOURCE_DIR}/src/memory_internal.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/memory_failure_support.h")

add_executable(gklib-memory-size-safety-test memory_size_safety.c)
target_compile_definitions(gklib-memory-size-safety-test PRIVATE
  GKLIB_STATIC_DEFINE)
target_include_directories(gklib-memory-size-safety-test PRIVATE
  "${PROJECT_SOURCE_DIR}/include"
  "${GKLIB_GENERATED_INCLUDE_DIR}")
set_property(SOURCE memory_size_safety.c PROPERTY OBJECT_DEPENDS
  "${PROJECT_SOURCE_DIR}/src/mcore.c"
  "${PROJECT_SOURCE_DIR}/src/memory.c"
  "${PROJECT_SOURCE_DIR}/src/memory_internal.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/memory_failure_support.h")
if(NOT WIN32)
  gklib_test_link_threads(gklib-memory-size-safety-test)
endif()

add_executable(gklib-error-errno-test error_errno.c)
target_link_libraries(gklib-error-errno-test PRIVATE GKlib::GKlib)

# Recovery protocol checks link the real library; allocator fixtures above
# include controlled source implementations and cannot verify host ownership.
add_executable(gklib-recovery-test recovery.c)
target_link_libraries(gklib-recovery-test PRIVATE GKlib::GKlib)
if(NOT WIN32)
  gklib_test_link_threads(gklib-recovery-test)
endif()

# Query injection crosses the real static producer's external libc reference.
# Shared-library binding is different and is covered by normal consumer tests.
get_target_property(_gklib_frame_test_type GKlib::GKlib TYPE)
if(NOT WIN32 AND _gklib_frame_test_type STREQUAL "STATIC_LIBRARY")
  gklib_test_check_link_wrap(_gklib_frame_test_wrap)
  if(_gklib_frame_test_wrap)
    add_executable(gklib-frame-setup-failure-test frame_setup_failure.c)
    target_link_libraries(gklib-frame-setup-failure-test PRIVATE
      GKlib::GKlib)
    gklib_test_link_threads(gklib-frame-setup-failure-test)
    target_compile_definitions(gklib-frame-setup-failure-test PRIVATE
      _POSIX_C_SOURCE=200809L)
    target_link_options(gklib-frame-setup-failure-test PRIVATE
      "LINKER:--wrap=pthread_sigmask")
    set_target_properties(gklib-frame-setup-failure-test PROPERTIES
      C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    gklib_test_add_runtime(NAME GKlib.frame-setup-failure
      TARGET gklib-frame-setup-failure-test)
    set_tests_properties(GKlib.frame-setup-failure PROPERTIES TIMEOUT 30)
  endif()
endif()

# Strict failure exits need subprocess assertions; ordinary recovery fixtures
# cannot continue after a producer leaves a failed release binding active.
if(_gklib_frame_test_type STREQUAL "STATIC_LIBRARY")
  gklib_test_check_link_wrap(_gklib_release_test_wrap)
  if(_gklib_release_test_wrap)
    set(_gklib_release_cases queue-create queue-init queue2-create)
    if(NOT WIN32)
      list(APPEND _gklib_release_cases modern legacy)
    endif()
    foreach(_language IN ITEMS c cxx)
      set(_source recovery_release_failure.c)
      if(_language STREQUAL "cxx")
        set(_source recovery_release_failure.cpp)
      endif()
      set(_target "gklib-release-failure-${_language}-test")
      add_executable(${_target} ${_source})
      target_link_libraries(${_target} PRIVATE GKlib::GKlib)
      target_link_options(${_target} PRIVATE "LINKER:--wrap=gk_siguntrap")
      set_target_properties(${_target} PROPERTIES
        C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO
        CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
        INTERPROCEDURAL_OPTIMIZATION FALSE)
      set(_gklib_release_configs ${CMAKE_BUILD_TYPE} ${CMAKE_CONFIGURATION_TYPES})
      list(REMOVE_DUPLICATES _gklib_release_configs)
      foreach(_config IN LISTS _gklib_release_configs)
        string(TOUPPER "${_config}" _upper)
        set_property(TARGET ${_target} PROPERTY
          "INTERPROCEDURAL_OPTIMIZATION_${_upper}" FALSE)
      endforeach()
      if(NOT WIN32)
        target_compile_definitions(${_target} PRIVATE
          GKLIB_TEST_POSIX_RELEASE _POSIX_C_SOURCE=200809L)
        gklib_test_link_threads(${_target})
        target_link_options(${_target} PRIVATE "LINKER:--wrap=pthread_sigmask")
      endif()
      foreach(_case IN LISTS _gklib_release_cases)
        foreach(_mode IN ITEMS 0 1)
          gklib_test_add_runtime(NAME "GKlib.release-failure.${_language}.${_case}.${_mode}"
            TARGET ${_target} ARGS ${_case} ${_mode} EXPECT_FAILURE)
          set_tests_properties("GKlib.release-failure.${_language}.${_case}.${_mode}"
            PROPERTIES TIMEOUT 30)
        endforeach()
      endforeach()
    endforeach()
    if(NOT WIN32)
      gklib_test_add_runtime(NAME GKlib.release-failure.public-retry
        TARGET gklib-release-failure-c-test ARGS public-retry)
      set_tests_properties(GKlib.release-failure.public-retry PROPERTIES TIMEOUT 30)
    endif()
  endif()
endif()

add_executable(gklib-string-growth-failure-test string_growth_failure.c)
target_include_directories(gklib-string-growth-failure-test PRIVATE
  "${PROJECT_SOURCE_DIR}/include"
  "${GKLIB_GENERATED_INCLUDE_DIR}")
target_link_libraries(gklib-string-growth-failure-test PRIVATE GKlib::GKlib)

add_executable(gklib-tokenizer-failure-test tokenizer_failure.c)
target_include_directories(gklib-tokenizer-failure-test PRIVATE
  "${PROJECT_SOURCE_DIR}/include"
  "${GKLIB_GENERATED_INCLUDE_DIR}")
target_link_libraries(gklib-tokenizer-failure-test PRIVATE GKlib::GKlib)

add_executable(gklib-io-fs-hardening-test io_fs_hardening.c)
target_link_libraries(gklib-io-fs-hardening-test PRIVATE GKlib::GKlib)

add_executable(gklib-getline-fallback-failure-test
  getline_fallback_failure.c)
target_link_libraries(gklib-getline-fallback-failure-test PRIVATE
  GKlib::GKlib)

add_executable(gklib-fs-link-safety-test fs_link_safety.c)
target_link_libraries(gklib-fs-link-safety-test PRIVATE GKlib::GKlib)

add_executable(gklib-graph-csr-hardening-test graph_csr_hardening.c)
target_link_libraries(gklib-graph-csr-hardening-test PRIVATE GKlib::GKlib)

add_executable(gklib-writer-failure-hardening-test writer_failure_hardening.c)
target_link_libraries(gklib-writer-failure-hardening-test PRIVATE GKlib::GKlib)

# Wrap the private transaction primitives in one translation unit so real
# flush, close, and commit failures are deterministic on every toolchain.
add_executable(gklib-output-transaction-failure-test
  output_transaction_failure.c)
target_link_libraries(gklib-output-transaction-failure-test PRIVATE
  GKlib::GKlib)

if(MSVC OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  foreach(test IN ITEMS
      gklib-binary-reader-short-read-test
      gklib-fs-link-safety-test
      gklib-getline-fallback-failure-test
      gklib-graph-csr-hardening-test
      gklib-io-fs-hardening-test
      gklib-output-transaction-failure-test
      gklib-seq-reader-test
      gklib-writer-failure-hardening-test)
    target_compile_definitions(${test} PRIVATE _CRT_SECURE_NO_WARNINGS)
  endforeach()
endif()

set_target_properties(
  gklib-allocation-matrix-test
  gklib-binary-reader-short-read-test
  gklib-cache-failure-recovery-test
  gklib-cache-size-safety-test
  gklib-error-errno-test
  gklib-graph-csr-hardening-test
  gklib-fs-link-safety-test
  gklib-getline-fallback-failure-test
  gklib-io-fs-hardening-test
  gklib-htable-safety-test
  gklib-memory-failure-recovery-test
  gklib-memory-size-safety-test
  gklib-output-transaction-failure-test
  gklib-pqueue-failure-recovery-test
  gklib-regex-realloc-test
  gklib-recovery-test
  gklib-seq-reader-test
  gklib-sort-sentinel-test
  gklib-string-growth-failure-test
  gklib-tokenizer-failure-test
  gklib-writer-failure-hardening-test
  PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS YES)

foreach(test IN ITEMS
    allocation-matrix
    binary-reader-short-read
    cache-failure-recovery
    cache-size-safety
    error-errno
    graph-csr-hardening
    fs-link-safety
    getline-fallback-failure
    io-fs-hardening
    htable-safety
    memory-failure-recovery
    memory-size-safety
    output-transaction-failure
    pqueue-failure-recovery
    regex-realloc
    recovery
    sort-sentinel
    string-growth-failure
    tokenizer-failure
    writer-failure-hardening)
  gklib_test_add_runtime(NAME "GKlib.${test}"
    TARGET "gklib-${test}-test"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties("GKlib.${test}" PROPERTIES TIMEOUT 30)
endforeach()

set_tests_properties(GKlib.fs-link-safety PROPERTIES
  SKIP_REGULAR_EXPRESSION "GKLIB_LINK_TEST_SKIPPED")

gklib_test_add_runtime(NAME GKlib.seq-reader
  TARGET gklib-seq-reader-test
  ARGS "${CMAKE_CURRENT_BINARY_DIR}/seq-reader.pssm"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(GKlib.seq-reader PROPERTIES TIMEOUT 30)

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

  add_test(NAME GKlib.app-cli
    COMMAND "${CMAKE_COMMAND}"
      -DGKGRAPH=$<TARGET_FILE:gkgraph>
      -DCSRCNV=$<TARGET_FILE:csrcnv>
      -DGKUNIQ=$<TARGET_FILE:gkuniq>
      -DCMPNBRS=$<TARGET_FILE:cmpnbrs>
      -DFIS=$<TARGET_FILE:fis>
      -DGKRW=$<TARGET_FILE:gkrw>
      -DGRKX=$<TARGET_FILE:grKx>
      -DM2MNBRS=$<TARGET_FILE:m2mnbrs>
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/app-cli
      "-DCROSSCOMPILING=${CMAKE_CROSSCOMPILING}"
      "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/app_cli.cmake")
  set_tests_properties(GKlib.app-cli PROPERTIES TIMEOUT 30)
  if(CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
    set_tests_properties(GKlib.app-cli PROPERTIES
      SKIP_REGULAR_EXPRESSION
        "^GKLIB_RUNTIME_SKIPPED: no cross-compiling emulator")
  endif()

  add_test(NAME GKlib.splatt2svd-hardening
    COMMAND "${CMAKE_COMMAND}"
      -DSPLATT2SVD=$<TARGET_FILE:splatt2svd>
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/splatt2svd-hardening
      "-DCROSSCOMPILING=${CMAKE_CROSSCOMPILING}"
      "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/splatt2svd_hardening.cmake")
  set_tests_properties(GKlib.splatt2svd-hardening PROPERTIES TIMEOUT 30)
  if(CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
    set_tests_properties(GKlib.splatt2svd-hardening PROPERTIES
      SKIP_REGULAR_EXPRESSION
        "^GKLIB_RUNTIME_SKIPPED: no cross-compiling emulator")
  endif()
endif()

# The C++ consumer verifies public headers and thread-related link requirements.
add_executable(gklib-cxx-consumer-test cxx_consumer.cpp)
target_compile_features(gklib-cxx-consumer-test PRIVATE cxx_std_11)
target_link_libraries(gklib-cxx-consumer-test PRIVATE GKlib::GKlib)
gklib_test_link_threads(gklib-cxx-consumer-test)
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
      gklib-cache-size-safety-test
      gklib-cxx-consumer-test
      gklib-error-errno-test
      gklib-fs-link-safety-test
      gklib-getline-fallback-failure-test
      gklib-graph-csr-hardening-test
      gklib-io-fs-hardening-test
      gklib-output-transaction-failure-test
      gklib-recovery-test
      gklib-seq-reader-test
      gklib-sort-sentinel-test
      gklib-string-growth-failure-test
      gklib-tokenizer-failure-test
      gklib-writer-failure-hardening-test)
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
      "-DTEST_INITIAL_CACHE=${_gklib_test_initial_cache}"
      "-DGENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}"
      "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
      "-DTEST_CONFIG=$<CONFIG>"
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
