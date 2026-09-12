#-------------------------------------------------------------------------------
# API AND PROGRAM TESTS
#-------------------------------------------------------------------------------
add_executable(gklib-api-test api.c)
target_link_libraries(gklib-api-test PRIVATE GKlib::GKlib)
add_test(NAME GKlib.api COMMAND gklib-api-test)

if(GKLIB_BUILD_PROGRAMS)
  add_test(NAME GKlib.gksort COMMAND gksort)
  set_tests_properties(GKlib.gksort PROPERTIES TIMEOUT 30
    FAIL_REGULAR_EXPRESSION "error at index")
endif()

# The C++ consumer verifies public headers and thread-related link requirements.
find_package(Threads REQUIRED)
add_executable(gklib-cxx-consumer-test cxx_consumer.cpp)
target_compile_features(gklib-cxx-consumer-test PRIVATE cxx_std_11)
target_link_libraries(gklib-cxx-consumer-test PRIVATE GKlib::GKlib Threads::Threads)
add_test(NAME GKlib.cxx-consumer COMMAND gklib-cxx-consumer-test)

set_tests_properties(GKlib.api GKlib.cxx-consumer PROPERTIES
  TIMEOUT 30
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# Copy runtime dependencies beside build-tree test executables on Windows.
if(WIN32)
  foreach(test gklib-api-test gklib-cxx-consumer-test)
    add_custom_command(TARGET ${test} POST_BUILD
      COMMAND ${CMAKE_COMMAND}
        "-DLIBRARIES=$<TARGET_RUNTIME_DLLS:${test}>"
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
  set(_gklib_integration_script
    "${CMAKE_CURRENT_SOURCE_DIR}/integration/run_fixture.cmake")
  set(_gklib_integration_work
    "${CMAKE_CURRENT_BINARY_DIR}/integration")
  set(_gklib_integration_common
    "-DGKLIB_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DMETIS_SOURCE_DIR=${PROJECT_SOURCE_DIR}/../.."
    "-DWORK_ROOT=${_gklib_integration_work}"
    "-DGENERATOR=${CMAKE_GENERATOR}"
    "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
    "-DC_COMPILER=${CMAKE_C_COMPILER}"
    "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCONFIG=Release")

  function(gklib_add_integration_test name scenario)
    add_test(NAME "GKlib.integration.${name}"
      COMMAND "${CMAKE_COMMAND}"
        ${_gklib_integration_common}
        "-DSCENARIO=${scenario}"
        -P "${_gklib_integration_script}")
    set_tests_properties("GKlib.integration.${name}" PROPERTIES
      PROCESSORS 2
      RUN_SERIAL TRUE
      TIMEOUT 300)
  endfunction()

  gklib_add_integration_test(openmp-cxx openmp-cxx)

  # The separately selected MSVC LLVM runtime needs its own development and
  # execution environment. Exercise it when the caller selected that SDK.
  if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" AND
      (OpenMP_RUNTIME_MSVC STREQUAL "llvm" OR
       OpenMP_C_FLAGS MATCHES "[-/]openmp:llvm($|[ ;])"))
    gklib_add_integration_test(openmp-llvm openmp-llvm)
  endif()

  # Optional language discovery is confined to the integration suite. Normal
  # builds never require Fortran or change the parent's enabled languages.
  include(CheckLanguage)
  check_language(Fortran)
  if(CMAKE_Fortran_COMPILER)
    list(APPEND _gklib_integration_common
      "-DFORTRAN_COMPILER=${CMAKE_Fortran_COMPILER}")
    gklib_add_integration_test(openmp-fortran openmp-fortran)

    # Reuse the explicitly selected PCRE SDK; do not download a test dependency.
    if(GKLIB_REGEX_BACKEND_RESOLVED STREQUAL "PCRE")
      foreach(_pcre_variable IN ITEMS PCRE_INCLUDE_DIR PCRE_POSIX_LIBRARY PCRE_LIBRARY)
        list(APPEND _gklib_integration_common
          "-D${_pcre_variable}=${${_pcre_variable}}")
      endforeach()
      gklib_add_integration_test(pcre-fortran pcre-fortran)
      list(APPEND _gklib_integration_common
        "-DPARENT_PCRE_LINKAGE=${GKLIB_CONFIG_PCRE_LINKAGE}")
      gklib_add_integration_test(pcre-parent-target pcre-parent-target)
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
  add_test(NAME gklib.install-ownership
    COMMAND "${CMAKE_COMMAND}"
      "-DTEST_SOURCE_DIR=${PROJECT_SOURCE_DIR}" "-DTEST_PROJECT=GKlib"
      "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/install-ownership"
      "-DGENERATOR=${CMAKE_GENERATOR}"
      "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
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
      "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
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
      "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
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
      "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
      "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
      "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
      "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
      "-DMODULE_FILE=${PROJECT_SOURCE_DIR}/cmake/GKlibCRT.cmake"
      -DCHECK_FUNCTION=gklib_check_shared_crt
      -DFIXTURE_PROJECT=GKLIB
      -P "${CMAKE_CURRENT_SOURCE_DIR}/crt/verify.cmake")
  set_tests_properties(gklib.shared-crt-policy PROPERTIES TIMEOUT 300)
endif()


# Maintenance regressions exercise merge behavior and runtime SDK discovery.
# Repository prose and formatting are reviewed separately from CTest.
if(GKLIB_BUILD_DEVELOPER_TESTING)
  find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)

  add_test(NAME gklib.upstream-tool
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
      -s "${CMAKE_CURRENT_SOURCE_DIR}/upstream" -p "test*.py")
  add_test(NAME gklib.intel-runtime
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
      -s "${CMAKE_CURRENT_SOURCE_DIR}/intelruntime" -p "test*.py")
  set_tests_properties(gklib.upstream-tool gklib.intel-runtime PROPERTIES
    ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1;CMAKE_COMMAND=${CMAKE_COMMAND}"
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}" TIMEOUT 120)
endif()
