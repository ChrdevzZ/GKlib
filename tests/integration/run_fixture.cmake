cmake_minimum_required(VERSION 3.24)

#-------------------------------------------------------------------------------
# INPUT AND WORKSPACE VALIDATION
#-------------------------------------------------------------------------------
foreach(_required IN ITEMS
    GKLIB_SOURCE_DIR WORK_ROOT GENERATOR TEST_INITIAL_CACHE SCENARIO)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()
if(NOT DEFINED CONFIG)
  message(FATAL_ERROR "CONFIG must be defined; an empty value is a valid configuration")
endif()
if(NOT EXISTS "${TEST_INITIAL_CACHE}")
  message(FATAL_ERROR "Test initial cache does not exist: ${TEST_INITIAL_CACHE}")
endif()

set(_scenarios
  openmp-cxx
  openmp-fortran
  openmp-llvm
  pcre-package
  pcre-fortran
  pcre-parent-target
  assertions
  openmp-cache
  subproject
  static-direct
  bundled-uninstall
  regex-link
  template-consumer)
if(NOT SCENARIO IN_LIST _scenarios)
  message(FATAL_ERROR "Unknown integration scenario: ${SCENARIO}")
endif()

get_filename_component(GKLIB_SOURCE_DIR "${GKLIB_SOURCE_DIR}" ABSOLUTE)
get_filename_component(WORK_ROOT "${WORK_ROOT}" ABSOLUTE)
if(WORK_ROOT STREQUAL GKLIB_SOURCE_DIR OR
  NOT WORK_ROOT MATCHES "[/\\\\]integration($|[/\\\\])")
  message(FATAL_ERROR "Refusing unsafe integration work directory: ${WORK_ROOT}")
endif()

get_filename_component(_work "${WORK_ROOT}/${SCENARIO}" ABSOLUTE)
cmake_path(IS_PREFIX WORK_ROOT "${_work}" NORMALIZE _inside_work_root)
if(NOT _inside_work_root)
  message(FATAL_ERROR "Integration scenario escaped the work directory")
endif()
file(REMOVE_RECURSE "${_work}")
file(MAKE_DIRECTORY "${_work}")

#-------------------------------------------------------------------------------
# TOOLCHAIN FORWARDING
#-------------------------------------------------------------------------------
set(_generator_args -G "${GENERATOR}")
if(DEFINED GENERATOR_INSTANCE AND NOT GENERATOR_INSTANCE STREQUAL "")
  list(APPEND _generator_args
    "-DCMAKE_GENERATOR_INSTANCE=${GENERATOR_INSTANCE}")
endif()
if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
  list(APPEND _generator_args -A "${GENERATOR_PLATFORM}")
endif()
if(DEFINED GENERATOR_TOOLSET AND NOT GENERATOR_TOOLSET STREQUAL "")
  list(APPEND _generator_args -T "${GENERATOR_TOOLSET}")
endif()

set(_compiler_args -C "${TEST_INITIAL_CACHE}")
if(DEFINED C_COMPILER AND NOT C_COMPILER STREQUAL "")
  list(APPEND _compiler_args "-DCMAKE_C_COMPILER=${C_COMPILER}")
endif()
if(DEFINED CXX_COMPILER AND NOT CXX_COMPILER STREQUAL "")
  list(APPEND _compiler_args "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
endif()

#-------------------------------------------------------------------------------
# COMMAND AND ASSERTION HELPERS
#-------------------------------------------------------------------------------
function(run_checked description)
  cmake_parse_arguments(PARSE_ARGV 1 _command "" "" "")
  execute_process(
    COMMAND ${_command_UNPARSED_ARGUMENTS}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR
      "${description} failed (${_result})\n${_stdout}\n${_stderr}")
  endif()
endfunction()

function(build_target binary_dir)
  set(_command "${CMAKE_COMMAND}" --build "${binary_dir}")
  if(NOT CONFIG STREQUAL "")
    list(APPEND _command --config "${CONFIG}")
  endif()
  list(APPEND _command --parallel 2 ${ARGN})
  run_checked("build in ${binary_dir}" ${_command})
endfunction()

function(read_target_path path_file output_variable)
  if(NOT EXISTS "${path_file}")
    message(FATAL_ERROR "Target path file was not generated: ${path_file}")
  endif()
  file(READ "${path_file}" _target_path)
  string(STRIP "${_target_path}" _target_path)
  if(NOT EXISTS "${_target_path}")
    message(FATAL_ERROR "Built target does not exist: ${_target_path}")
  endif()
  set(${output_variable} "${_target_path}" PARENT_SCOPE)
endfunction()

function(expect_exit description executable should_succeed)
  # Runtime fixtures own any temporary data beside their built executable.
  get_filename_component(_run_directory "${executable}" DIRECTORY)
  if(CROSSCOMPILING AND NOT EMULATOR)
    set_property(GLOBAL PROPERTY GKLIB_RUNTIME_WAS_SKIPPED TRUE)
    return()
  endif()
  set(_command "${executable}")
  if(CROSSCOMPILING)
    list(PREPEND _command ${EMULATOR})
  endif()
  execute_process(
    COMMAND ${_command}
    WORKING_DIRECTORY "${_run_directory}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr)
  if(should_succeed AND NOT _result EQUAL 0)
    message(FATAL_ERROR
      "${description} unexpectedly failed (${_result})\n${_stdout}\n${_stderr}")
  elseif(NOT should_succeed AND _result EQUAL 0)
    message(FATAL_ERROR "${description} unexpectedly exited successfully")
  endif()
endfunction()

function(run_nested_tests description binary_dir)
  if(CROSSCOMPILING AND NOT EMULATOR)
    set_property(GLOBAL PROPERTY GKLIB_RUNTIME_WAS_SKIPPED TRUE)
    return()
  endif()
  set(_command "${CMAKE_CTEST_COMMAND}" --test-dir "${binary_dir}")
  if(NOT CONFIG STREQUAL "")
    list(APPEND _command -C "${CONFIG}")
  endif()
  list(APPEND _command --output-on-failure --parallel 2)
  run_checked("${description}" ${_command})
endfunction()

#-------------------------------------------------------------------------------
# SCENARIOS
#-------------------------------------------------------------------------------
# Each branch owns an isolated producer/consumer flow under the validated root.
if(SCENARIO STREQUAL "subproject")
  set(_build "${_work}/build")
  run_checked("configure GKlib as a subproject"
    "${CMAKE_COMMAND}" -S "${GKLIB_SOURCE_DIR}/tests/subproject" -B "${_build}"
    ${_generator_args} ${_compiler_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}")
  build_target("${_build}")
  read_target_path("${_build}/consumer-${CONFIG}.path" _consumer_executable)
  expect_exit("GKlib subproject consumer" "${_consumer_executable}" TRUE)

elseif(SCENARIO STREQUAL "openmp-cache")
  # find_library() consults an existing variable even with NO_CACHE. Poison
  # both the former generic name and the new private name, then prove that the
  # importer performs a fresh search without altering either parent cache.
  set(_fixture "${_work}/fixture")
  set(_module_dir "${_fixture}/cmake")
  set(_runtime_dir "${_fixture}/runtime/lib")
  file(MAKE_DIRECTORY "${_module_dir}" "${_runtime_dir}")
  file(COPY "${GKLIB_SOURCE_DIR}/cmake/GKlibOpenMP.cmake"
    DESTINATION "${_module_dir}")
  file(WRITE "${_fixture}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.24)
project(OpenMPCacheIsolation LANGUAGES C)

set(_runtime_name
  "${CMAKE_STATIC_LIBRARY_PREFIX}gklib_fixture_runtime${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(_runtime "${CMAKE_CURRENT_SOURCE_DIR}/runtime/lib/${_runtime_name}")
set(_unrelated "${CMAKE_CURRENT_SOURCE_DIR}/unrelated${CMAKE_STATIC_LIBRARY_SUFFIX}")
file(WRITE "${_runtime}" "")
file(WRITE "${_unrelated}" "")
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GKlibOpenMP-RELEASE.cmake"
  "set(_gklib_openmp_names_RELEASE \"${_runtime_name}\")\n")

set(library "${_unrelated}" CACHE FILEPATH "Parent cache sentinel")
set(_gklib_openmp_runtime_library "${_unrelated}"
  CACHE FILEPATH "Private-name cache sentinel")

add_library(GKlib::GKlib UNKNOWN IMPORTED)
set_property(TARGET GKlib::GKlib PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_property(TARGET GKlib::GKlib PROPERTY IMPORTED_LOCATION_RELEASE "${_runtime}")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GKlibOpenMP.cmake")
gklib_openmp_import(GKlib::GKlib)

get_target_property(_resolved GKlib::OpenMPRuntime IMPORTED_LOCATION_RELEASE)
if(NOT _resolved STREQUAL _runtime)
  message(FATAL_ERROR
    "OpenMP runtime resolved to parent cache entry: ${_resolved}")
endif()
foreach(_cache IN ITEMS library _gklib_openmp_runtime_library)
  get_property(_value CACHE ${_cache} PROPERTY VALUE)
  if(NOT _value STREQUAL _unrelated)
    message(FATAL_ERROR "Importer modified parent cache ${_cache}: ${_value}")
  endif()
endforeach()
]=])
  run_checked("OpenMP runtime cache isolation"
    "${CMAKE_COMMAND}" -S "${_fixture}" -B "${_work}/build"
    ${_generator_args} ${_compiler_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DOpenMP_ROOT=${_fixture}/runtime")

elseif(SCENARIO STREQUAL "regex-link")
  # Supply declarations without relying on a platform regex installation.
  # Only the positive case defines the functions required by the link probe.
  file(WRITE "${_work}/regex.h" [=[
typedef int regex_t;
#define REG_EXTENDED 1
#define regcomp gklib_fixture_regcomp
#define regexec gklib_fixture_regexec
#define regfree gklib_fixture_regfree
int regcomp(regex_t *, const char *, int);
int regexec(regex_t *, const char *, int, void *, int);
void regfree(regex_t *);
#ifdef PROVIDE_REGEX
int regcomp(regex_t *r, const char *s, int f) { return 0; }
int regexec(regex_t *r, const char *s, int n, void *m, int f) { return 0; }
void regfree(regex_t *r) {}
#endif
]=])
  file(WRITE "${_work}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.24)
project(RegexLinkFixture LANGUAGES C)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "${INHERITED_TARGET_TYPE}")
set(CMAKE_REQUIRED_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}")
if(PROVIDE_REGEX)
  set(CMAKE_REQUIRED_DEFINITIONS -DPROVIDE_REGEX)
endif()
set(GKLIB_BUILD_PROGRAMS OFF)
set(GKLIB_BUILD_TESTING OFF)
set(GKLIB_INSTALL OFF)
add_subdirectory("${GKLIB_SOURCE_DIR}" gklib)
if(NOT CMAKE_TRY_COMPILE_TARGET_TYPE STREQUAL INHERITED_TARGET_TYPE)
  message(FATAL_ERROR "GKlib changed its parent's try-compile policy")
endif()
if(NOT PROVIDE_REGEX AND NOT GKLIB_REGEX_BACKEND_RESOLVED STREQUAL "GKREGEX")
  message(FATAL_ERROR "Unavailable system regex did not select bundled regex")
endif()
]=])
  foreach(_target_type IN ITEMS EXECUTABLE STATIC_LIBRARY)
    set(_configure "${CMAKE_COMMAND}" -S "${_work}" -B "${_work}/${_target_type}"
      ${_generator_args} ${_compiler_args} "-DCMAKE_BUILD_TYPE=${CONFIG}"
      "-DGKLIB_SOURCE_DIR=${GKLIB_SOURCE_DIR}"
      "-DINHERITED_TARGET_TYPE=${_target_type}")
    run_checked("linkable system regex with ${_target_type} parent"
      ${_configure} -DPROVIDE_REGEX=ON -DGKLIB_REGEX_BACKEND=SYSTEM)

    # Reuse the same binary tree to prove that a formerly successful probe is
    # invalidated when the required definitions no longer provide the runtime.
    execute_process(COMMAND ${_configure}
      -DPROVIDE_REGEX=OFF -DGKLIB_REGEX_BACKEND=SYSTEM
      RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_result EQUAL 0 OR NOT _stderr MATCHES "requires regex.h and linkable POSIX")
      message(FATAL_ERROR "Missing regex runtime was not rejected:\n${_stdout}\n${_stderr}")
    endif()
    foreach(_backend IN ITEMS AUTO GKREGEX)
      run_checked("bundled regex fallback with ${_target_type} parent"
        ${_configure} -DPROVIDE_REGEX=OFF "-DGKLIB_REGEX_BACKEND=${_backend}")
    endforeach()
  endforeach()

elseif(SCENARIO MATCHES "^openmp-(cxx|fortran|llvm)$")
  set(_language CXX)
  set(_consumer_compilers ${_compiler_args})
  set(_openmp_options)
  if(SCENARIO STREQUAL "openmp-llvm")
    # CMake 3.24 redetects both entries when either is missing. State the
    # known MSVC contract: the flag selects LLVM and objects name its library.
    list(APPEND _openmp_options
      -DOpenMP_C_FLAGS=-openmp:llvm -DOpenMP_C_LIB_NAMES:STRING=)
  endif()
  if(SCENARIO STREQUAL "openmp-fortran")
    set(_language Fortran)
    set(_consumer_compilers ${_compiler_args}
      "-DCMAKE_Fortran_COMPILER=${FORTRAN_COMPILER}")
  endif()

  # Each language consumes both library types. An absent development SDK must
  # fail for a static archive and remain unnecessary for a resolved shared one.
  foreach(_shared IN ITEMS OFF ON)
    set(_work "${WORK_ROOT}/${SCENARIO}/${_shared}")
    set(_producer "${_work}/producer")
    set(_prefix "${_work}/prefix")
    run_checked("configure OpenMP GKlib (${_shared})"
      "${CMAKE_COMMAND}" -S "${GKLIB_SOURCE_DIR}" -B "${_producer}"
      ${_generator_args} ${_compiler_args} ${_openmp_options}
      "-DCMAKE_BUILD_TYPE=${CONFIG}"
      "-DGKLIB_BUILD_SHARED_LIBS=${_shared}"
      -DGKLIB_IPO=OFF
      -DGKLIB_BUILD_PROGRAMS=OFF
      -DGKLIB_BUILD_TESTING=OFF
      -DGKLIB_INSTALL=ON
      -DGKLIB_OPENMP=ON)
    build_target("${_producer}")
    run_checked("install OpenMP GKlib (${_shared})"
      "${CMAKE_COMMAND}" --install "${_producer}"
      --config "${CONFIG}" --prefix "${_prefix}")

    set(_consumer "${_work}/consumer")
    set(_runtime_options)
    if(SCENARIO STREQUAL "openmp-llvm" AND NOT _shared)
      # Only the actual LLVM import library is available during package
      # discovery. An incorrect VCOMP record must not pass on the host SDK.
      set(_runtime_name libomp.lib)
      if(CONFIG STREQUAL "Debug")
        set(_runtime_name libompd.lib)
      endif()
      find_library(_runtime_library NAMES "${_runtime_name}" HINTS ENV LIB REQUIRED)
      file(MAKE_DIRECTORY "${_work}/runtime-sdk/lib")
      file(COPY "${_runtime_library}" DESTINATION "${_work}/runtime-sdk/lib")
      list(APPEND _runtime_options "-DCONSUMER_RUNTIME_ROOT=${_work}/runtime-sdk")
    endif()
    run_checked("configure ${_language}-only OpenMP package consumer"
      "${CMAKE_COMMAND}"
      -S "${GKLIB_SOURCE_DIR}/tests/integration/openmp-cxx"
      -B "${_consumer}" ${_generator_args} ${_consumer_compilers} ${_runtime_options}
      "-DGKLIB_CONSUMER_LANGUAGE=${_language}"
      "-DCMAKE_BUILD_TYPE=${CONFIG}"
      "-DCMAKE_PREFIX_PATH=${_prefix}")
    build_target("${_consumer}")
    read_target_path("${_consumer}/consumer-${CONFIG}.path" _consumer_executable)
    expect_exit("${_language}-only OpenMP package consumer" "${_consumer_executable}" TRUE)

    if(NOT _shared)
      # Add an unused producer configuration to the real installed package.
      # Its deliberately absent runtime must matter only when the consumer
      # explicitly maps to that configuration, not during ordinary discovery.
      file(GLOB_RECURSE _package_configs "${_prefix}/*/GKlibConfig.cmake")
      list(GET _package_configs 0 _package_config)
      get_filename_component(_package_dir "${_package_config}" DIRECTORY)
      set(_extra_config "${_package_dir}/GKlibTargets-unavailable.cmake")
      file(WRITE "${_extra_config}" [=[
get_target_property(_configs GKlib::GKlib IMPORTED_CONFIGURATIONS)
list(GET _configs 0 _config)
get_target_property(_archive GKlib::GKlib IMPORTED_LOCATION_${_config})
set_property(TARGET GKlib::GKlib APPEND PROPERTY IMPORTED_CONFIGURATIONS UNAVAILABLE)
set_property(TARGET GKlib::GKlib PROPERTY IMPORTED_LOCATION_UNAVAILABLE "${_archive}")
]=])
      file(WRITE "${_package_dir}/GKlibOpenMP-UNAVAILABLE.cmake"
        "set(_gklib_openmp_names_UNAVAILABLE gklib_test_missing_openmp_runtime)\n")
      run_checked("ignore unused OpenMP producer configuration"
        "${CMAKE_COMMAND}"
        -S "${GKLIB_SOURCE_DIR}/tests/integration/openmp-cxx"
        -B "${_work}/unused-config" ${_generator_args} ${_consumer_compilers}
        ${_runtime_options}
        "-DCMAKE_BUILD_TYPE=${CONFIG}" "-DCMAKE_PREFIX_PATH=${_prefix}"
        "-DGKLIB_CONSUMER_LANGUAGE=${_language}")
      build_target("${_work}/unused-config")
      read_target_path("${_work}/unused-config/consumer-${CONFIG}.path"
        _consumer_executable)
      expect_exit("unused OpenMP configuration consumer" "${_consumer_executable}" TRUE)

      string(TOUPPER "${CONFIG}" _config_upper)
      execute_process(COMMAND "${CMAKE_COMMAND}"
        -S "${GKLIB_SOURCE_DIR}/tests/integration/openmp-cxx"
        -B "${_work}/mapped-missing-config" ${_generator_args} ${_consumer_compilers}
        "-DCMAKE_BUILD_TYPE=${CONFIG}" "-DCMAKE_PREFIX_PATH=${_prefix}"
        "-DCMAKE_MAP_IMPORTED_CONFIG_${_config_upper}=Unavailable"
        "-DGKLIB_CONSUMER_LANGUAGE=${_language}"
        RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
      set(_diagnostic "${_stdout}\n${_stderr}")
      string(REPLACE "\r" " " _diagnostic "${_diagnostic}")
      string(REPLACE "\n" " " _diagnostic "${_diagnostic}")
      if(_result EQUAL 0 OR NOT _diagnostic MATCHES
          "GKlib UNAVAILABLE requires producer OpenMP runtime +gklib_test_missing_openmp_runtime")
        message(FATAL_ERROR
          "Explicit OpenMP configuration mapping was not enforced:\n${_stdout}\n${_stderr}")
      endif()
    endif()

    execute_process(COMMAND "${CMAKE_COMMAND}"
      -S "${GKLIB_SOURCE_DIR}/tests/integration/openmp-cxx"
      -B "${_work}/missing-runtime" ${_generator_args} ${_consumer_compilers}
      "-DCMAKE_BUILD_TYPE=${CONFIG}" "-DCMAKE_PREFIX_PATH=${_prefix}"
      "-DGKLIB_CONSUMER_LANGUAGE=${_language}" -DCONSUMER_HIDE_RUNTIME=ON
      RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(_shared)
      if(NOT _result EQUAL 0)
        message(FATAL_ERROR "Shared GKlib requested an OpenMP SDK:\n${_stdout}\n${_stderr}")
      endif()
      build_target("${_work}/missing-runtime")
    # Intel archives also need the C runtime SDK, which may be reported first
    # when all development-library search roots are deliberately hidden.
    elseif(_result EQUAL 0 OR NOT _stderr MATCHES
        "requires producer OpenMP runtime|Could NOT find IntelRuntime")
      message(FATAL_ERROR "Missing producer runtime was not diagnosed:\n${_stdout}\n${_stderr}")
    endif()
  endforeach()

elseif(SCENARIO STREQUAL "pcre-package")
  if(NOT PARENT_PCRE_LINKAGE MATCHES "^(STATIC|SHARED)$")
    message(FATAL_ERROR "PARENT_PCRE_LINKAGE must be STATIC or SHARED")
  endif()

  set(_sdk_args)
  foreach(_variable IN ITEMS PCRE_INCLUDE_DIR PCRE_POSIX_LIBRARY PCRE_LIBRARY)
    list(APPEND _sdk_args "-D${_variable}=${${_variable}}")
  endforeach()
  set(_producer "${_work}/producer")
  set(_prefix "${_work}/prefix")
  run_checked("configure PCRE GKlib for C++ package consumption"
    "${CMAKE_COMMAND}" -S "${GKLIB_SOURCE_DIR}" -B "${_producer}"
    ${_generator_args} ${_compiler_args} ${_sdk_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DPCRE_LINKAGE=${PARENT_PCRE_LINKAGE}"
    -DGKLIB_REGEX_BACKEND=PCRE
    -DGKLIB_IPO=OFF
    -DGKLIB_BUILD_SHARED_LIBS=OFF
    -DGKLIB_BUILD_PROGRAMS=OFF
    -DGKLIB_BUILD_TESTING=OFF
    -DGKLIB_INSTALL=ON)
  build_target("${_producer}")
  run_checked("install PCRE GKlib for C++ package consumption"
    "${CMAKE_COMMAND}" --install "${_producer}"
    --config "${CONFIG}" --prefix "${_prefix}")

  set(_consumer "${_work}/consumer")
  run_checked("configure C++ PCRE package consumer"
    "${CMAKE_COMMAND}" -S "${GKLIB_SOURCE_DIR}/tests/pcre-package"
    -B "${_consumer}" ${_generator_args} ${_compiler_args} ${_sdk_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DCMAKE_PREFIX_PATH=${_prefix}"
    "-DEXPECT_PCRE_LINKAGE=${PARENT_PCRE_LINKAGE}")
  build_target("${_consumer}")
  run_nested_tests("run C++ PCRE package consumer" "${_consumer}")

elseif(SCENARIO STREQUAL "pcre-fortran")
  # The installed PCRE interface must be usable without enabling C or C++ in
  # the consumer. Exercise real regex operations, not only package discovery.
  set(_sdk_args)
  foreach(_variable IN ITEMS PCRE_INCLUDE_DIR PCRE_POSIX_LIBRARY PCRE_LIBRARY)
    list(APPEND _sdk_args "-D${_variable}=${${_variable}}")
  endforeach()
  set(_producer "${_work}/producer")
  set(_prefix "${_work}/prefix")
  run_checked("configure PCRE GKlib"
    "${CMAKE_COMMAND}" -S "${GKLIB_SOURCE_DIR}" -B "${_producer}"
    ${_generator_args} ${_compiler_args} ${_sdk_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}" -DGKLIB_REGEX_BACKEND=PCRE
    -DGKLIB_IPO=OFF -DGKLIB_BUILD_PROGRAMS=OFF -DGKLIB_BUILD_TESTING=OFF
    -DGKLIB_BUILD_SHARED_LIBS=OFF -DGKLIB_INSTALL=ON)
  build_target("${_producer}")
  run_checked("install PCRE GKlib" "${CMAKE_COMMAND}" --install "${_producer}"
    --config "${CONFIG}" --prefix "${_prefix}")
  set(_consumer "${_work}/consumer")
  run_checked("configure Fortran-only PCRE package consumer"
    "${CMAKE_COMMAND}" -S "${GKLIB_SOURCE_DIR}/tests/integration/openmp-cxx"
    -B "${_consumer}" ${_generator_args} ${_compiler_args} ${_sdk_args}
    "-DCMAKE_Fortran_COMPILER=${FORTRAN_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${CONFIG}" "-DCMAKE_PREFIX_PATH=${_prefix}"
    -DGKLIB_CONSUMER_LANGUAGE=Fortran)
  build_target("${_consumer}")
  read_target_path("${_consumer}/consumer-${CONFIG}.path" _consumer_executable)
  expect_exit("Fortran-only PCRE package consumer" "${_consumer_executable}" TRUE)

elseif(SCENARIO STREQUAL "pcre-parent-target")
  # Concrete and canonical parent targets can carry exact linkage metadata into
  # an installed package. An opaque interface remains usable for non-install
  # builds but must not be guessed when package metadata persists the decision.
  if(NOT PARENT_PCRE_LINKAGE MATCHES "^(STATIC|SHARED)$")
    message(FATAL_ERROR "PARENT_PCRE_LINKAGE must be STATIC or SHARED")
  endif()

  set(_sdk_args)
  foreach(_variable IN ITEMS PCRE_INCLUDE_DIR PCRE_POSIX_LIBRARY PCRE_LIBRARY)
    list(APPEND _sdk_args "-D${_variable}=${${_variable}}")
  endforeach()
  if(WIN32 AND PARENT_PCRE_LINKAGE STREQUAL "SHARED")
    get_filename_component(_pcre_library_dir "${PCRE_LIBRARY}" DIRECTORY)
    find_file(_pcre_dll NAMES pcre.dll libpcre.dll libpcre-1.dll
      HINTS "${_pcre_library_dir}/../bin" NO_DEFAULT_PATH NO_CACHE)
    find_file(_pcre_posix_dll
      NAMES pcreposix.dll libpcreposix.dll libpcreposix-0.dll
      HINTS "${_pcre_library_dir}/../bin" NO_DEFAULT_PATH NO_CACHE)
    if(NOT _pcre_dll OR NOT _pcre_posix_dll)
      message(FATAL_ERROR "Shared PCRE fixture DLLs were not found")
    endif()
    list(APPEND _sdk_args
      "-DPCRE_DLL=${_pcre_dll}"
      "-DPCRE_POSIX_DLL=${_pcre_posix_dll}")
  endif()

  file(WRITE "${_work}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.24)
project(ParentPCREFixture LANGUAGES C)

foreach(_required IN ITEMS GKLIB_SOURCE_DIR PARENT_PCRE_LINKAGE
    PCRE_INCLUDE_DIR PCRE_POSIX_LIBRARY PCRE_LIBRARY)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

if(OPAQUE_PCRE_TARGET)
  add_library(PCRE::POSIX INTERFACE IMPORTED GLOBAL)
  set_target_properties(PCRE::POSIX PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PCRE_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${PCRE_POSIX_LIBRARY};${PCRE_LIBRARY}")
  if(WIN32 AND PARENT_PCRE_LINKAGE STREQUAL "STATIC")
    set_property(TARGET PCRE::POSIX APPEND PROPERTY
      INTERFACE_COMPILE_DEFINITIONS PCRE_STATIC)
  endif()
elseif(DIRECT_PCRE_TARGET)
  add_library(PCRE::pcre ${PARENT_PCRE_LINKAGE} IMPORTED GLOBAL)
  add_library(PCRE::POSIX ${PARENT_PCRE_LINKAGE} IMPORTED GLOBAL)
  if(WIN32 AND PARENT_PCRE_LINKAGE STREQUAL "SHARED")
    set_target_properties(PCRE::pcre PROPERTIES
      IMPORTED_IMPLIB "${PCRE_LIBRARY}"
      IMPORTED_LOCATION "${PCRE_DLL}")
    set_target_properties(PCRE::POSIX PROPERTIES
      IMPORTED_IMPLIB "${PCRE_POSIX_LIBRARY}"
      IMPORTED_LOCATION "${PCRE_POSIX_DLL}")
  else()
    set_target_properties(PCRE::pcre PROPERTIES
      IMPORTED_LOCATION "${PCRE_LIBRARY}")
    set_target_properties(PCRE::POSIX PROPERTIES
      IMPORTED_LOCATION "${PCRE_POSIX_LIBRARY}")
  endif()
  set_target_properties(PCRE::pcre PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PCRE_INCLUDE_DIR}")
  set_target_properties(PCRE::POSIX PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PCRE_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES PCRE::pcre)
  if(WIN32 AND PARENT_PCRE_LINKAGE STREQUAL "STATIC")
    set_property(TARGET PCRE::pcre APPEND PROPERTY
      INTERFACE_COMPILE_DEFINITIONS PCRE_STATIC)
    set_property(TARGET PCRE::POSIX APPEND PROPERTY
      INTERFACE_COMPILE_DEFINITIONS PCRE_STATIC)
  endif()
else()
  add_library(PCRE::pcre ${PARENT_PCRE_LINKAGE} IMPORTED GLOBAL)
  add_library(PCRE::pcreposix ${PARENT_PCRE_LINKAGE} IMPORTED GLOBAL)
  if(WIN32 AND PARENT_PCRE_LINKAGE STREQUAL "SHARED")
    set_target_properties(PCRE::pcre PROPERTIES
      IMPORTED_IMPLIB "${PCRE_LIBRARY}"
      IMPORTED_LOCATION "${PCRE_DLL}")
    set_target_properties(PCRE::pcreposix PROPERTIES
      IMPORTED_IMPLIB "${PCRE_POSIX_LIBRARY}"
      IMPORTED_LOCATION "${PCRE_POSIX_DLL}")
  else()
    set_target_properties(PCRE::pcre PROPERTIES
      IMPORTED_LOCATION "${PCRE_LIBRARY}")
    set_target_properties(PCRE::pcreposix PROPERTIES
      IMPORTED_LOCATION "${PCRE_POSIX_LIBRARY}")
  endif()
  set_target_properties(PCRE::pcre PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PCRE_INCLUDE_DIR}")
  set_target_properties(PCRE::pcreposix PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PCRE_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES PCRE::pcre)
  if(WIN32 AND PARENT_PCRE_LINKAGE STREQUAL "STATIC")
    set_property(TARGET PCRE::pcre APPEND PROPERTY
      INTERFACE_COMPILE_DEFINITIONS PCRE_STATIC)
    set_property(TARGET PCRE::pcreposix APPEND PROPERTY
      INTERFACE_COMPILE_DEFINITIONS PCRE_STATIC)
  endif()
  add_library(PCRE::POSIX INTERFACE IMPORTED GLOBAL)
  set_target_properties(PCRE::POSIX PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PCRE_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES PCRE::pcreposix)
endif()

set(GKLIB_REGEX_BACKEND PCRE)
set(GKLIB_IPO OFF)
set(GKLIB_BUILD_SHARED_LIBS OFF)
set(GKLIB_BUILD_PROGRAMS OFF)
set(GKLIB_BUILD_TESTING OFF)
set(GKLIB_INSTALL "${INSTALL_GKLIB}")
add_subdirectory("${GKLIB_SOURCE_DIR}" gklib)
]=])

  set(_parent_args ${_sdk_args}
    "-DGKLIB_SOURCE_DIR=${GKLIB_SOURCE_DIR}"
    "-DPARENT_PCRE_LINKAGE=${PARENT_PCRE_LINKAGE}")
  set(_producer "${_work}/producer")
  set(_prefix "${_work}/prefix")
  run_checked("configure GKlib with canonical parent PCRE targets"
    "${CMAKE_COMMAND}" -S "${_work}" -B "${_producer}"
    ${_generator_args} ${_compiler_args} ${_parent_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}" -DINSTALL_GKLIB=ON)
  build_target("${_producer}")
  run_checked("install parent-target PCRE GKlib"
    "${CMAKE_COMMAND}" --install "${_producer}"
    --config "${CONFIG}" --prefix "${_prefix}")

  set(_consumer "${_work}/consumer")
  run_checked("configure Fortran consumer of parent-target PCRE package"
    "${CMAKE_COMMAND}" -S "${GKLIB_SOURCE_DIR}/tests/integration/openmp-cxx"
    -B "${_consumer}" ${_generator_args} ${_compiler_args} ${_sdk_args}
    "-DCMAKE_Fortran_COMPILER=${FORTRAN_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${CONFIG}" "-DCMAKE_PREFIX_PATH=${_prefix}"
    -DGKLIB_CONSUMER_LANGUAGE=Fortran)
  build_target("${_consumer}")
  read_target_path("${_consumer}/consumer-${CONFIG}.path" _consumer_executable)
  expect_exit("Fortran consumer of parent-target PCRE package"
    "${_consumer_executable}" TRUE)

  set(_direct_producer "${_work}/direct-producer")
  set(_direct_prefix "${_work}/direct-prefix")
  run_checked("configure GKlib with concrete PCRE::POSIX parent target"
    "${CMAKE_COMMAND}" -S "${_work}" -B "${_direct_producer}"
    ${_generator_args} ${_compiler_args} ${_parent_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}" -DDIRECT_PCRE_TARGET=ON -DINSTALL_GKLIB=ON)
  build_target("${_direct_producer}")
  run_checked("install direct parent-target PCRE GKlib"
    "${CMAKE_COMMAND}" --install "${_direct_producer}"
    --config "${CONFIG}" --prefix "${_direct_prefix}")

  set(_direct_consumer "${_work}/direct-consumer")
  run_checked("configure Fortran consumer of direct PCRE parent target"
    "${CMAKE_COMMAND}" -S "${GKLIB_SOURCE_DIR}/tests/integration/openmp-cxx"
    -B "${_direct_consumer}" ${_generator_args} ${_compiler_args} ${_sdk_args}
    "-DCMAKE_Fortran_COMPILER=${FORTRAN_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${CONFIG}" "-DCMAKE_PREFIX_PATH=${_direct_prefix}"
    -DGKLIB_CONSUMER_LANGUAGE=Fortran)
  build_target("${_direct_consumer}")
  read_target_path("${_direct_consumer}/consumer-${CONFIG}.path"
    _direct_consumer_executable)
  expect_exit("Fortran consumer of direct PCRE parent target"
    "${_direct_consumer_executable}" TRUE)

  execute_process(COMMAND "${CMAKE_COMMAND}"
    -S "${_work}" -B "${_work}/opaque-install"
    ${_generator_args} ${_compiler_args} ${_parent_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}" -DOPAQUE_PCRE_TARGET=ON -DINSTALL_GKLIB=ON
    RESULT_VARIABLE _result OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
  if(_result EQUAL 0 OR NOT "${_stdout}\n${_stderr}" MATCHES
      "PCRE_LINKAGE=STATIC or SHARED")
    message(FATAL_ERROR
      "Opaque parent PCRE linkage was not rejected for install:\n${_stdout}\n${_stderr}")
  endif()

  set(_explicit_producer "${_work}/explicit-producer")
  set(_explicit_prefix "${_work}/explicit-prefix")
  run_checked("configure install with explicit opaque parent PCRE linkage"
    "${CMAKE_COMMAND}" -S "${_work}" -B "${_explicit_producer}"
    ${_generator_args} ${_compiler_args} ${_parent_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}" -DOPAQUE_PCRE_TARGET=ON
    "-DPCRE_LINKAGE=${PARENT_PCRE_LINKAGE}" -DINSTALL_GKLIB=ON)
  build_target("${_explicit_producer}")
  run_checked("install explicit parent-target PCRE GKlib"
    "${CMAKE_COMMAND}" --install "${_explicit_producer}"
    --config "${CONFIG}" --prefix "${_explicit_prefix}")

  set(_explicit_consumer "${_work}/explicit-consumer")
  run_checked("configure Fortran consumer of explicit parent PCRE package"
    "${CMAKE_COMMAND}" -S "${GKLIB_SOURCE_DIR}/tests/integration/openmp-cxx"
    -B "${_explicit_consumer}" ${_generator_args} ${_compiler_args} ${_sdk_args}
    "-DCMAKE_Fortran_COMPILER=${FORTRAN_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${CONFIG}" "-DCMAKE_PREFIX_PATH=${_explicit_prefix}"
    -DGKLIB_CONSUMER_LANGUAGE=Fortran)
  build_target("${_explicit_consumer}")
  read_target_path("${_explicit_consumer}/consumer-${CONFIG}.path"
    _explicit_consumer_executable)
  expect_exit("Fortran consumer of explicit parent PCRE package"
    "${_explicit_consumer_executable}" TRUE)

  run_checked("configure non-install GKlib with opaque parent PCRE target"
    "${CMAKE_COMMAND}" -S "${_work}" -B "${_work}/opaque-build"
    ${_generator_args} ${_compiler_args} ${_parent_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}" -DOPAQUE_PCRE_TARGET=ON -DINSTALL_GKLIB=OFF)
  build_target("${_work}/opaque-build")

elseif(SCENARIO STREQUAL "template-consumer")
  # Cover both static and shared installed template interfaces.
  foreach(_shared IN ITEMS OFF ON)
    set(_producer "${_work}/${_shared}/producer")
    set(_prefix "${_work}/${_shared}/prefix")
    run_checked("configure GKlib template producer (${_shared})"
      "${CMAKE_COMMAND}" -S "${GKLIB_SOURCE_DIR}" -B "${_producer}"
      ${_generator_args} ${_compiler_args}
      "-DCMAKE_BUILD_TYPE=${CONFIG}"
      "-DGKLIB_BUILD_SHARED_LIBS=${_shared}"
      -DGKLIB_BUILD_PROGRAMS=OFF
      -DGKLIB_BUILD_TESTING=OFF
      -DGKLIB_INSTALL=ON)
    build_target("${_producer}")
    run_checked("install GKlib template producer (${_shared})"
      "${CMAKE_COMMAND}" --install "${_producer}"
      --config "${CONFIG}" --prefix "${_prefix}")
    set(_consumer "${_work}/${_shared}/consumer")
    run_checked("configure C/C++ template consumers (${_shared})"
      "${CMAKE_COMMAND}" -S "${GKLIB_SOURCE_DIR}/tests/template-consumer"
      -B "${_consumer}" ${_generator_args} ${_compiler_args}
      "-DCMAKE_BUILD_TYPE=${CONFIG}" "-DCMAKE_PREFIX_PATH=${_prefix}")
    build_target("${_consumer}")
    run_nested_tests("run C/C++ template consumers (${_shared})"
      "${_consumer}")
  endforeach()

elseif(SCENARIO STREQUAL "assertions")
  # Exercise both assertion tiers for every ON/OFF policy combination. The
  # verifier requires the deliberate SIGABRT path, its marker and GKlib's own
  # diagnostic instead of treating an arbitrary process failure as success.
  set(_fixture "${GKLIB_SOURCE_DIR}/tests/integration/assertions")
  set(_verifier "${_fixture}/verify.cmake")

  function(verify_assertion description executable kind expected)
    if(CROSSCOMPILING AND NOT EMULATOR)
      set_property(GLOBAL PROPERTY GKLIB_RUNTIME_WAS_SKIPPED TRUE)
      return()
    endif()
    run_checked("${description}"
      "${CMAKE_COMMAND}"
      "-DDESCRIPTION=${description}"
      "-DEXECUTABLE=${executable}"
      "-DASSERTION_KIND=${kind}"
      "-DEXPECT_TRIGGER=${expected}"
      "-DEXECUTION_PREFIX=${EMULATOR}"
      -P "${_verifier}")
  endfunction()

  function(reject_assertion_protocol description executable kind argument reason)
    execute_process(
      COMMAND "${CMAKE_COMMAND}"
        "-DDESCRIPTION=${description}"
        "-DEXECUTABLE=${executable}"
        "-DEXECUTABLE_ARGUMENTS=${argument}"
        "-DEXECUTION_PREFIX=${EMULATOR}"
        "-DASSERTION_KIND=${kind}"
        -DEXPECT_TRIGGER=ON
        -DEXECUTION_TIMEOUT=1
        -P "${_verifier}"
      RESULT_VARIABLE _result
      OUTPUT_VARIABLE _stdout
      ERROR_VARIABLE _stderr)
    set(_output "${_stdout}\n${_stderr}")
    string(REPLACE "\r" " " _output "${_output}")
    string(REPLACE "\n" " " _output "${_output}")
    if(_result EQUAL 0 OR NOT _output MATCHES "${reason}")
      message(FATAL_ERROR
        "Assertion protocol accepted ${description} (${_result})\n${_stdout}\n${_stderr}")
    endif()
  endfunction()

  foreach(_ordinary IN ITEMS OFF ON)
    foreach(_expensive IN ITEMS OFF ON)
      string(TOLOWER "${_ordinary}-${_expensive}" _combination)
      set(_assertion_build "${_work}/${_combination}")
      run_checked("configure assertions ${_ordinary}/${_expensive}"
        "${CMAKE_COMMAND}" -S "${_fixture}" -B "${_assertion_build}"
        ${_generator_args} ${_compiler_args}
        "-DCMAKE_BUILD_TYPE=${CONFIG}"
        "-DGKLIB_SOURCE_DIR=${GKLIB_SOURCE_DIR}"
        "-DGKLIB_ASSERTIONS=${_ordinary}"
        "-DGKLIB_ASSERTIONS_EXPENSIVE=${_expensive}")
      build_target("${_assertion_build}")

      foreach(_kind IN ITEMS ordinary expensive)
        read_target_path(
          "${_assertion_build}/assert-${_kind}-${CONFIG}.path"
          _assertion_executable)
        if(_kind STREQUAL "ordinary")
          set(_expected "${_ordinary}")
        else()
          set(_expected "${_expensive}")
        endif()
        verify_assertion(
          "${_kind} assertion with ordinary=${_ordinary}, expensive=${_expensive}"
          "${_assertion_executable}" "${_kind}" "${_expected}")
      endforeach()
    endforeach()
  endforeach()

  if(NOT CROSSCOMPILING)
    # Prove the verifier rejects each false-positive mode that the former
    # nonzero-only check accepted. These host-process protocol cases are not
    # repeated through emulators, whose process and timeout semantics differ.
    read_target_path("${_work}/on-on/assert-ordinary-${CONFIG}.path"
      _protocol_executable)
    reject_assertion_protocol("unrelated exit" "${_protocol_executable}"
      ordinary unrelated-exit "did not exit through")
    reject_assertion_protocol("marker-only exit" "${_protocol_executable}"
      ordinary marker-only "did not emit the GKlib assertion diagnostic")
    reject_assertion_protocol("wrong assertion tier" "${_protocol_executable}"
      ordinary wrong-tier "did not emit the expected start marker")
    reject_assertion_protocol("ordinary crash" "${_protocol_executable}"
      ordinary plain-crash "did not exit through")
    reject_assertion_protocol("timeout" "${_protocol_executable}"
      ordinary timeout "Process +terminated due to timeout")
    reject_assertion_protocol("startup failure" "${_work}/missing-assertion-test"
      ordinary "" "did not emit the expected start marker")
  endif()

elseif(SCENARIO STREQUAL "static-direct")
  # Windows direct-header consumers must receive the static export definition.
  if(NOT WIN32)
    message(FATAL_ERROR "The static direct-header regression is Windows-specific")
  endif()
  set(_producer "${_work}/producer")
  set(_prefix "${_work}/prefix")
  run_checked("configure static GKlib"
    "${CMAKE_COMMAND}" -S "${GKLIB_SOURCE_DIR}" -B "${_producer}"
    ${_generator_args} ${_compiler_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}"
    -DGKLIB_BUILD_SHARED_LIBS=OFF
    -DGKLIB_BUILD_PROGRAMS=OFF
    -DGKLIB_BUILD_TESTING=OFF
    -DGKLIB_INSTALL=ON)
  build_target("${_producer}")
  run_checked("install static GKlib"
    "${CMAKE_COMMAND}" --install "${_producer}"
    --config "${CONFIG}" --prefix "${_prefix}")

  set(_consumer "${_work}/consumer")
  run_checked("configure raw static-header consumer"
    "${CMAKE_COMMAND}"
    -S "${GKLIB_SOURCE_DIR}/tests/integration/static-direct"
    -B "${_consumer}" ${_generator_args} ${_compiler_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DGKLIB_PREFIX=${_prefix}")
  build_target("${_consumer}")
  read_target_path("${_consumer}/consumer-${CONFIG}.path" _consumer_executable)
  expect_exit("raw static-header consumer" "${_consumer_executable}" TRUE)

elseif(SCENARIO STREQUAL "bundled-uninstall")
  # A nested GKlib uninstall must preserve METIS and unrelated parent files.
  if(NOT DEFINED METIS_SOURCE_DIR OR METIS_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "METIS_SOURCE_DIR is required for bundled-uninstall")
  endif()
  get_filename_component(METIS_SOURCE_DIR "${METIS_SOURCE_DIR}" ABSOLUTE)
  set(_build "${_work}/build")
  if(UNIX)
    set(_prefix "/usr/local")
    set(_destdir "${_work}/destdir")
    set(_installed_root "${_destdir}${_prefix}")
  else()
    set(_prefix "${_work}/prefix")
    set(_destdir "")
    set(_installed_root "${_prefix}")
  endif()
  run_checked("configure bundled METIS and GKlib install"
    "${CMAKE_COMMAND}" -S "${METIS_SOURCE_DIR}" -B "${_build}"
    ${_generator_args} ${_compiler_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DCMAKE_INSTALL_PREFIX=${_prefix}"
    -DMETIS_BUILD_SHARED_LIBS=OFF
    -DMETIS_BUILD_PROGRAMS=OFF
    -DMETIS_BUILD_TESTING=OFF
    -DMETIS_INSTALL=ON
    -DMETIS_GKLIB_PROVIDER=SOURCE
    "-DMETIS_GKLIB_SOURCE_DIR=${GKLIB_SOURCE_DIR}"
    -DGKLIB_BUILD_SHARED_LIBS=OFF
    -DGKLIB_BUILD_PROGRAMS=OFF
    -DGKLIB_BUILD_TESTING=OFF
    -DGKLIB_INSTALL=ON)
  build_target("${_build}")
  if(UNIX)
    run_checked("DESTDIR install bundled METIS and GKlib"
      "${CMAKE_COMMAND}" -E env "DESTDIR=${_destdir}"
      "${CMAKE_COMMAND}" --install "${_build}" --config "${CONFIG}")
  else()
    run_checked("install bundled METIS and GKlib"
      "${CMAKE_COMMAND}" --install "${_build}" --config "${CONFIG}")
  endif()

  set(_gklib_header "${_installed_root}/include/GKlib.h")
  set(_metis_header "${_installed_root}/include/metis.h")
  set(_sentinel "${_installed_root}/share/unrelated-sentinel.txt")
  foreach(_installed IN ITEMS "${_gklib_header}" "${_metis_header}")
    if(NOT EXISTS "${_installed}")
      message(FATAL_ERROR "Expected installed file is missing: ${_installed}")
    endif()
  endforeach()
  file(MAKE_DIRECTORY "${_installed_root}/share")
  file(WRITE "${_sentinel}" "preserve me\n")
  file(APPEND "${_build}/install_manifest.txt"
    "\n${_prefix}/share/unrelated-sentinel.txt\n")

  file(GLOB _metis_libraries "${_installed_root}/lib/*metis*")
  if(NOT _metis_libraries)
    message(FATAL_ERROR "No installed METIS library found under ${_installed_root}/lib")
  endif()
  if(UNIX)
    run_checked("DESTDIR GKlib uninstall from bundled manifest"
      "${CMAKE_COMMAND}" -E env "DESTDIR=${_destdir}"
      "${CMAKE_COMMAND}" --build "${_build}" --config "${CONFIG}"
      --target gklib-uninstall)
  else()
    run_checked("GKlib uninstall from bundled manifest"
      "${CMAKE_COMMAND}" --build "${_build}" --config "${CONFIG}"
      --target gklib-uninstall)
  endif()

  if(EXISTS "${_gklib_header}")
    message(FATAL_ERROR "GKlib header survived gklib-uninstall")
  endif()
  file(GLOB_RECURSE _gklib_files "${_installed_root}/*GKlib*")
  if(_gklib_files)
    message(FATAL_ERROR "GKlib files survived uninstall: ${_gklib_files}")
  endif()
  foreach(_preserved IN ITEMS "${_metis_header}" "${_sentinel}" ${_metis_libraries})
    if(NOT EXISTS "${_preserved}")
      message(FATAL_ERROR "gklib-uninstall removed unrelated file: ${_preserved}")
    endif()
  endforeach()

else()
  message(FATAL_ERROR "Unknown integration scenario: ${SCENARIO}")
endif()

get_property(_runtime_was_skipped GLOBAL PROPERTY GKLIB_RUNTIME_WAS_SKIPPED)
if(_runtime_was_skipped)
  message("GKLIB_RUNTIME_SKIPPED: no cross-compiling emulator")
endif()
