cmake_minimum_required(VERSION 3.24)

foreach(required IN ITEMS
    MODULE_FILE FUNCTION_PREFIX SKIP_MARKER WORK_DIR GENERATOR)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

cmake_path(ABSOLUTE_PATH WORK_DIR NORMALIZE OUTPUT_VARIABLE work)
if(NOT work MATCHES "[/\\]test-support$")
  message(FATAL_ERROR "Unsafe test-support work directory: ${work}")
endif()
file(MAKE_DIRECTORY "${work}")


function(run_checked description)
  cmake_parse_arguments(PARSE_ARGV 1 command "" "" "")
  execute_process(
    COMMAND ${command_UNPARSED_ARGUMENTS}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR
      "${description} failed (${result})\n${output}\n${error}")
  endif()
endfunction()


# Load the cache written by the project helper and compare the resulting CMake
# values. This exercises serialization of list, path, generator-expression and
# custom-linker inputs without inspecting the generated file's spelling.
include("${MODULE_FILE}")
set(TEST_PLATFORM_PAYLOAD [=[alpha;C:\SDK Path\$<CONFIG>@TOKEN@]=])
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
  TEST_PLATFORM_PAYLOAD CMAKE_SYSTEM_NAME CMAKE_SYSTEM_PROCESSOR)
if(NOT CMAKE_CROSSCOMPILING)
  set(CMAKE_SYSTEM_NAME "${CMAKE_HOST_SYSTEM_NAME}")
  set(CMAKE_SYSTEM_PROCESSOR test-host)
endif()
set(CMAKE_LINKER_TYPE test_custom)
set(CMAKE_C_USING_LINKER_test_custom "-fuse-ld=lld")
set(CMAKE_C_USING_LINKER_MODE FLAG)
set(CMAKE_C_LINKER_LAUNCHER "launcher;--trace")
set(initial_cache "${work}/initial-cache.cmake")
cmake_language(CALL "${FUNCTION_PREFIX}_test_write_initial_cache"
  "${initial_cache}")

set(loader "${work}/load-cache.cmake")
file(CONFIGURE OUTPUT "${loader}" CONTENT [=[
if(NOT "@CMAKE_CROSSCOMPILING@" AND
    (DEFINED CMAKE_SYSTEM_NAME OR DEFINED CMAKE_SYSTEM_PROCESSOR))
  message(FATAL_ERROR "Native test cache forced a target system identity")
endif()
file(WRITE "${OUTPUT_FILE}"
  "${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES}\n${TEST_PLATFORM_PAYLOAD}\n"
  "${CMAKE_LINKER_TYPE}\n${CMAKE_C_USING_LINKER_test_custom}\n"
  "${CMAKE_C_USING_LINKER_MODE}\n${CMAKE_C_LINKER_LAUNCHER}")
]=] @ONLY NEWLINE_STYLE LF)
set(loaded "${work}/loaded.txt")
run_checked("load generated test cache"
  "${CMAKE_COMMAND}" -C "${initial_cache}"
  "-DOUTPUT_FILE=${loaded}" -P "${loader}")
file(READ "${loaded}" loaded_payload)
string(CONCAT expected_payload
  "TEST_PLATFORM_PAYLOAD;CMAKE_SYSTEM_NAME;CMAKE_SYSTEM_PROCESSOR\n"
  "${TEST_PLATFORM_PAYLOAD}\ntest_custom\n-fuse-ld=lld\nFLAG\nlauncher;--trace")
if(NOT loaded_payload STREQUAL expected_payload)
  message(FATAL_ERROR
    "Test cache changed a forwarded platform or linker value\n"
    "expected: ${expected_payload}\nactual: ${loaded_payload}")
endif()

# Exercise the project runtime wrapper itself. Native failures must not become
# skips, successful output must remain available to CTest diagnostics, and a
# missing cross target must fail before the no-emulator skip boundary.
set(bad_script "${work}/bad-runtime.cmake")
file(WRITE "${bad_script}"
  "message(\"${SKIP_MARKER}\")\nmessage(FATAL_ERROR \"deliberate failure\")\n")
set(output_script "${work}/runtime-output.cmake")
file(WRITE "${output_script}" "message(\"error at index\")\n")
set(recorder_script "${work}/record-emulator.cmake")
file(WRITE "${recorder_script}" [=[
file(WRITE "${RECORDER_FILE}" "invoked")
set(command)
set(index 4)
while(index LESS CMAKE_ARGC)
  list(APPEND command "${CMAKE_ARGV${index}}")
  math(EXPR index "${index} + 1")
endwhile()
execute_process(COMMAND ${command} RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "emulated command failed: ${result}")
endif()
]=])

set(fixture_source "${work}/fixture")
file(MAKE_DIRECTORY "${fixture_source}")
set(BAD_SCRIPT "${bad_script}")
set(OUTPUT_SCRIPT "${output_script}")
file(CONFIGURE OUTPUT "${fixture_source}/CMakeLists.txt" CONTENT [=[
cmake_minimum_required(VERSION 3.24)
project(TestSupportRuntime LANGUAGES NONE)
include("@MODULE_FILE@")
enable_testing()
add_executable(test-command IMPORTED GLOBAL)
if(TEST_MISSING_TARGET)
  set_property(TARGET test-command PROPERTY
    IMPORTED_LOCATION "@fixture_source@/missing-command")
  @FUNCTION_PREFIX@_test_add_runtime(NAME runtime TARGET test-command)
elseif(TEST_OUTPUT_TOKEN)
  set_property(TARGET test-command PROPERTY IMPORTED_LOCATION "@CMAKE_COMMAND@")
  @FUNCTION_PREFIX@_test_add_runtime(
    NAME runtime TARGET test-command ARGS -P "@OUTPUT_SCRIPT@")
  set_tests_properties(runtime PROPERTIES
    FAIL_REGULAR_EXPRESSION "error at index")
elseif(TEST_SUCCESS)
  set_property(TARGET test-command PROPERTY IMPORTED_LOCATION "@CMAKE_COMMAND@")
  @FUNCTION_PREFIX@_test_add_runtime(
    NAME runtime TARGET test-command ARGS -E true)
else()
  set_property(TARGET test-command PROPERTY IMPORTED_LOCATION "@CMAKE_COMMAND@")
  @FUNCTION_PREFIX@_test_add_runtime(
    NAME runtime TARGET test-command ARGS -P "@BAD_SCRIPT@")
endif()
]=] @ONLY NEWLINE_STYLE LF)

set(generator_args -G "${GENERATOR}")
if(MAKE_PROGRAM)
  list(APPEND generator_args "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
endif()
if(GENERATOR_INSTANCE)
  list(APPEND generator_args
    "-DCMAKE_GENERATOR_INSTANCE=${GENERATOR_INSTANCE}")
endif()
if(GENERATOR_PLATFORM)
  list(APPEND generator_args -A "${GENERATOR_PLATFORM}")
endif()
if(GENERATOR_TOOLSET)
  list(APPEND generator_args -T "${GENERATOR_TOOLSET}")
endif()
if(TEST_CONFIG AND GENERATOR MATCHES "Multi-Config|Visual Studio|Xcode")
  list(APPEND generator_args "-DCMAKE_CONFIGURATION_TYPES=${TEST_CONFIG}")
endif()

set(ctest_config)
if(TEST_CONFIG)
  list(APPEND ctest_config -C "${TEST_CONFIG}")
endif()

set(native_build "${work}/native")
run_checked("configure native runtime fixture"
  "${CMAKE_COMMAND}" -S "${fixture_source}" -B "${native_build}"
  ${generator_args})
execute_process(
  COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${native_build}"
    ${ctest_config} --output-on-failure
  RESULT_VARIABLE native_result
  OUTPUT_VARIABLE native_output
  ERROR_VARIABLE native_error)
if(native_result EQUAL 0 OR
    "${native_output}\n${native_error}" MATCHES "[ *]Skipped")
  message(FATAL_ERROR
    "Native failure was accepted as a skip\n${native_output}\n${native_error}")
endif()

set(output_build "${work}/runtime-output")
run_checked("configure runtime output fixture"
  "${CMAKE_COMMAND}" -S "${fixture_source}" -B "${output_build}"
  ${generator_args} -DTEST_OUTPUT_TOKEN=ON)
execute_process(
  COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${output_build}"
    ${ctest_config} --output-on-failure
  RESULT_VARIABLE output_result
  OUTPUT_VARIABLE output_output
  ERROR_VARIABLE output_error)
if(output_result EQUAL 0 OR
    NOT "${output_output}\n${output_error}" MATCHES "error at index")
  message(FATAL_ERROR
    "Runtime wrapper hid successful process output\n${output_output}\n${output_error}")
endif()

set(no_emulator_toolchain "${work}/no-emulator.cmake")
file(WRITE "${no_emulator_toolchain}" "set(CMAKE_SYSTEM_NAME Generic)\n")
set(no_emulator_build "${work}/cross-no-emulator")
run_checked("configure cross fixture without emulator"
  "${CMAKE_COMMAND}" -S "${fixture_source}" -B "${no_emulator_build}"
  ${generator_args} "-DCMAKE_TOOLCHAIN_FILE=${no_emulator_toolchain}")
execute_process(
  COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${no_emulator_build}"
    ${ctest_config} --output-on-failure
  RESULT_VARIABLE skip_result
  OUTPUT_VARIABLE skip_output
  ERROR_VARIABLE skip_error)
if(NOT skip_result EQUAL 0 OR
    NOT "${skip_output}\n${skip_error}" MATCHES "[ *]Skipped")
  message(FATAL_ERROR
    "Cross runtime was not reported as skipped\n${skip_output}\n${skip_error}")
endif()

set(missing_build "${work}/cross-missing-target")
run_checked("configure cross fixture with a missing target"
  "${CMAKE_COMMAND}" -S "${fixture_source}" -B "${missing_build}"
  ${generator_args} "-DCMAKE_TOOLCHAIN_FILE=${no_emulator_toolchain}"
  -DTEST_MISSING_TARGET=ON)
execute_process(
  COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${missing_build}"
    ${ctest_config} --output-on-failure
  RESULT_VARIABLE missing_result
  OUTPUT_VARIABLE missing_output
  ERROR_VARIABLE missing_error)
if(missing_result EQUAL 0 OR
    "${missing_output}\n${missing_error}" MATCHES "[ *]Skipped" OR
    NOT "${missing_output}\n${missing_error}" MATCHES "does not exist")
  message(FATAL_ERROR
    "Missing cross target was accepted as a runtime skip\n"
    "${missing_output}\n${missing_error}")
endif()

# A configured emulator is a list-valued command prefix. Prove that the wrapper
# invokes it and that the emulator subsequently runs the requested command.
set(recorder "${work}/emulator-invoked.txt")
file(REMOVE "${recorder}")
set(emulator_toolchain "${work}/emulator.cmake")
file(WRITE "${emulator_toolchain}"
  "set(CMAKE_SYSTEM_NAME Generic)\n"
  "set(CMAKE_CROSSCOMPILING_EMULATOR \"${CMAKE_COMMAND};-DRECORDER_FILE=${recorder};-P;${recorder_script}\")\n")
set(emulator_build "${work}/cross-emulator")
run_checked("configure cross fixture with emulator"
  "${CMAKE_COMMAND}" -S "${fixture_source}" -B "${emulator_build}"
  ${generator_args} "-DCMAKE_TOOLCHAIN_FILE=${emulator_toolchain}"
  -DTEST_SUCCESS=ON)
run_checked("emulated runtime"
  "${CMAKE_CTEST_COMMAND}" --test-dir "${emulator_build}"
  ${ctest_config} --output-on-failure)
if(NOT EXISTS "${recorder}")
  message(FATAL_ERROR "Configured emulator was not invoked")
endif()

# Repeated final-link probes must use current flags and leave parent state intact.
set(wrap_source "${work}/wrap-fixture")
file(MAKE_DIRECTORY "${wrap_source}")
string(TOUPPER "${FUNCTION_PREFIX}" WRAP_UPPER)
if(FUNCTION_PREFIX STREQUAL "metis")
  set(WRAP_MISSING metis_test_missing)
else()
  set(WRAP_MISSING gklib_frame_probe_missing)
endif()
file(CONFIGURE OUTPUT "${wrap_source}/CMakeLists.txt" CONTENT [=[
cmake_minimum_required(VERSION 3.24)
project(TestLinkWrap LANGUAGES C)
include("@MODULE_FILE@")
if(TEST_WRAP_CUSTOM_CONFIG)
  set(CMAKE_CONFIGURATION_TYPES Review CACHE STRING "Fixture configurations" FORCE)
  set(CMAKE_TRY_COMPILE_CONFIGURATION Review)
elseif(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Review)
endif()
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_REQUIRED_FLAGS parent-check-sentinel)
set(CMAKE_REQUIRED_LIBRARIES parent-library-sentinel)
set(@WRAP_UPPER@_TEST_WRAP_PARENT parent-cache-sentinel CACHE STRING "Parent result")
set(parent_platform "${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES}")
set(parent_configuration "${CMAKE_TRY_COMPILE_CONFIGURATION}")
get_cmake_property(before CACHE_VARIABLES)
@FUNCTION_PREFIX@_test_check_link_wrap(supported)
file(COPY "${CMAKE_BINARY_DIR}/link-wrap/link.log"
  DESTINATION "${CMAKE_BINARY_DIR}/initial")
if(supported)
  set(config "${CMAKE_TRY_COMPILE_CONFIGURATION}")
  if(NOT config)
    if(CMAKE_CONFIGURATION_TYPES)
      list(GET CMAKE_CONFIGURATION_TYPES 0 config)
    else()
      set(config "${CMAKE_BUILD_TYPE}")
    endif()
  endif()
  string(TOUPPER "${config}" upper)
  set(flags "${CMAKE_C_FLAGS_${upper}}")
  set(CMAKE_C_FLAGS_${upper}
    "${flags} -D@WRAP_MISSING@=test_wrap_renamed_reference")
  @FUNCTION_PREFIX@_test_check_link_wrap(changed)
  file(COPY "${CMAKE_BINARY_DIR}/link-wrap/link.log"
    DESTINATION "${CMAKE_BINARY_DIR}/changed-flags")
  if(changed)
    message(FATAL_ERROR "Probe reused a result after active flags changed")
  endif()
  set(CMAKE_C_FLAGS_${upper} "${flags}")
  @FUNCTION_PREFIX@_test_check_link_wrap(restored)
  if(NOT restored STREQUAL supported)
    message(FATAL_ERROR "Restored flags did not restore wrapping capability")
  endif()
endif()
if(NOT CMAKE_CROSSCOMPILING)
  set(emulator "${CMAKE_CROSSCOMPILING_EMULATOR}")
  set(CMAKE_CROSSCOMPILING_EMULATOR "${CMAKE_COMMAND};-E;false")
  @FUNCTION_PREFIX@_test_check_link_wrap(native_emulator)
  set(CMAKE_CROSSCOMPILING_EMULATOR "${emulator}")
  if(NOT native_emulator STREQUAL supported)
    message(FATAL_ERROR "Native emulator changed wrapping capability")
  endif()
endif()
get_cmake_property(after CACHE_VARIABLES)
if(NOT CMAKE_TRY_COMPILE_TARGET_TYPE STREQUAL "STATIC_LIBRARY" OR
    NOT CMAKE_REQUIRED_FLAGS STREQUAL "parent-check-sentinel" OR
    NOT CMAKE_REQUIRED_LIBRARIES STREQUAL "parent-library-sentinel" OR
    NOT @WRAP_UPPER@_TEST_WRAP_PARENT STREQUAL "parent-cache-sentinel" OR
    NOT "${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES}" STREQUAL "${parent_platform}" OR
    NOT "${CMAKE_TRY_COMPILE_CONFIGURATION}" STREQUAL "${parent_configuration}")
  message(FATAL_ERROR "Wrapping probe changed parent check state")
endif()
foreach(entry IN LISTS after)
  if(entry MATCHES "^@WRAP_UPPER@_TEST_WRAP_" AND NOT entry IN_LIST before)
    message(FATAL_ERROR "Wrapping probe left a temporary cache result")
  endif()
endforeach()
if(DEFINED TEST_WRAP_EXPECTED AND NOT supported STREQUAL TEST_WRAP_EXPECTED)
  message(FATAL_ERROR "Custom configuration changed wrapping capability")
endif()
file(WRITE "${CMAKE_BINARY_DIR}/wrap-supported.txt" "${supported}")
message(STATUS "Verified repeated wrapping capability: ${supported}")
]=] @ONLY NEWLINE_STYLE LF)
run_checked("configure repeated wrapping probe under an archive-only parent"
  "${CMAKE_COMMAND}" -S "${wrap_source}" -B "${work}/wrap-build"
  -C "${TEST_INITIAL_CACHE}" ${generator_args})
if(GENERATOR MATCHES "Multi-Config|Visual Studio|Xcode")
  file(READ "${work}/wrap-build/wrap-supported.txt" expected)
  run_checked("configure wrapping probe with a custom configuration"
    "${CMAKE_COMMAND}" -S "${wrap_source}" -B "${work}/wrap-custom-build"
    -C "${TEST_INITIAL_CACHE}" ${generator_args}
    -DTEST_WRAP_CUSTOM_CONFIG=ON "-DTEST_WRAP_EXPECTED=${expected}")
endif()

# Thread discovery must not mistake a compile-only parent probe for a usable
# recovery dependency, or inherit unrelated checks from the embedding project.
if(NOT WIN32)
  get_filename_component(test_source "${MODULE_FILE}" DIRECTORY)
  get_filename_component(gklib_source "${test_source}" DIRECTORY)
  set(thread_source "${work}/thread-source")
  file(MAKE_DIRECTORY "${thread_source}")
  file(CONFIGURE OUTPUT "${thread_source}/CMakeLists.txt" CONTENT [=[
cmake_minimum_required(VERSION 3.24)
project(ThreadMaskDependency LANGUAGES C)
include("@MODULE_FILE@")
gklib_test_check_link_wrap(wrapping)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
find_package(Threads REQUIRED)

function(thread_parent_snapshot output)
  set(snapshot "")
  get_cmake_property(entries CACHE_VARIABLES)
  list(SORT entries)
  foreach(entry IN LISTS entries)
    if(entry MATCHES "^(CMAKE_HAVE_.*(PTHREAD|CMA)|THREADS_HAVE_)")
      foreach(property IN ITEMS VALUE TYPE HELPSTRING)
        get_property(value CACHE "${entry}" PROPERTY "${property}")
        string(APPEND snapshot "${entry}.${property}=[${value}]\n")
      endforeach()
    endif()
  endforeach()
  foreach(property IN ITEMS INTERFACE_COMPILE_OPTIONS INTERFACE_LINK_LIBRARIES
      INTERFACE_LINK_OPTIONS INTERFACE_COMPILE_DEFINITIONS)
    get_target_property(value Threads::Threads "${property}")
    string(APPEND snapshot "${property}=[${value}]\n")
  endforeach()
  set(${output} "${snapshot}" PARENT_SCOPE)
endfunction()
thread_parent_snapshot(before)
set(CMAKE_REQUIRED_FLAGS -DGKLIB_PARENT_THREAD_SENTINEL=1)
set(CMAKE_REQUIRED_LIBRARIES parent-thread-library-sentinel)
set(GKLIB_BUILD_PROGRAMS OFF)
set(GKLIB_BUILD_TESTING OFF)
set(GKLIB_BUILD_INTEGRATION_TESTING OFF)
set(GKLIB_BUILD_DEVELOPER_TESTING OFF)
set(GKLIB_INSTALL ON)
set(GKLIB_IPO OFF)
if(TEST_THREAD_MODEL)
  set(GKLIB_BUILD_TESTING ON)
  # The real producer references a distinct external symbol. A separately
  # compiled libpthread model supplies it without replacing mask semantics.
  string(APPEND CMAKE_C_FLAGS " -Dpthread_sigmask=gklib_test_pthread_sigmask")
  string(APPEND CMAKE_EXE_LINKER_FLAGS " -L\"${TEST_THREAD_MODEL}\"")
  string(APPEND CMAKE_SHARED_LINKER_FLAGS " -L\"${TEST_THREAD_MODEL}\"")
endif()
if(TEST_REJECT_MASK_LINK AND wrapping)
  string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--wrap=pthread_sigmask")
endif()
add_subdirectory("@gklib_source@" gklib)
if(TEST_REJECT_MASK_LINK AND wrapping)
  message(FATAL_ERROR "An unavailable signal-mask link was accepted")
endif()
thread_parent_snapshot(after)
if(NOT before STREQUAL after OR
    NOT CMAKE_REQUIRED_FLAGS STREQUAL "-DGKLIB_PARENT_THREAD_SENTINEL=1" OR
    NOT CMAKE_TRY_COMPILE_TARGET_TYPE STREQUAL "STATIC_LIBRARY" OR
    NOT CMAKE_REQUIRED_LIBRARIES STREQUAL "parent-thread-library-sentinel")
  message(FATAL_ERROR "Thread discovery changed the parent check state")
endif()
add_executable(thread-mask main.c)
target_link_libraries(thread-mask PRIVATE GKlib::GKlib)
if(TEST_THREAD_MODEL)
  get_target_property(thread_links GKlib LINK_LIBRARIES)
  if(NOT "-pthread" IN_LIST thread_links AND
      NOT "-lpthread" IN_LIST thread_links)
    message(FATAL_ERROR "The producer did not retain its proven nonempty closure")
  endif()
  get_target_property(thread_options GKlib COMPILE_OPTIONS)
  if("-pthread" IN_LIST thread_links AND NOT "-pthread" IN_LIST thread_options)
    message(FATAL_ERROR "The producer lost its proven pthread compile option")
  endif()
endif()
enable_testing()
gklib_test_add_runtime(NAME thread-mask TARGET thread-mask)
file(WRITE "${CMAKE_BINARY_DIR}/thread-parent-state.txt" "${after}")
]=] @ONLY NEWLINE_STYLE LF)
  file(WRITE "${thread_source}/main.c"
    "#include <GKlib.h>\nint main(void) { return !gk_sigtrap() || !gk_siguntrap(); }\n")
  run_checked("configure signal-mask dependency under a compile-only parent"
    "${CMAKE_COMMAND}" -S "${thread_source}" -B "${work}/thread-build"
    -C "${TEST_INITIAL_CACHE}" ${generator_args})
  run_checked("link signal-mask consumer"
    "${CMAKE_COMMAND}" --build "${work}/thread-build"
    --config "${TEST_CONFIG}" --parallel 2)
  run_checked("run signal-mask consumer"
    "${CMAKE_CTEST_COMMAND}" --test-dir "${work}/thread-build"
    -C "${TEST_CONFIG}" --output-on-failure)

  # A PIC model archive with a unique mask symbol proves that a nonempty
  # closure reaches the producer, independent fixtures and package consumers.
  set(model_source "${work}/thread-model-source")
  file(MAKE_DIRECTORY "${model_source}")
  file(WRITE "${model_source}/model.c" "#include <signal.h>\n#include <pthread.h>\nint gklib_test_pthread_sigmask(int how, const sigset_t *in, sigset_t *out) { return pthread_sigmask(how, in, out); }\n")
  file(WRITE "${model_source}/empty.c" "int gklib_test_no_mask(void) { return 0; }\n")
  file(WRITE "${model_source}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.24)
project(ThreadClosureModel C)
if(TEST_EMPTY_MODEL)
  add_library(model STATIC empty.c)
else()
  add_library(model STATIC model.c)
endif()
set_target_properties(model PROPERTIES OUTPUT_NAME pthread
  POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(model PRIVATE _POSIX_C_SOURCE=200809L)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/model-path-$<CONFIG>.txt"
  CONTENT "$<TARGET_FILE_DIR:model>")
]=])
  run_checked("configure nonempty pthread model"
    "${CMAKE_COMMAND}" -S "${model_source}" -B "${work}/thread-model"
    -C "${TEST_INITIAL_CACHE}" ${generator_args})
  run_checked("build nonempty pthread model"
    "${CMAKE_COMMAND}" --build "${work}/thread-model"
    --config "${TEST_CONFIG}" --parallel 2)
  file(READ "${work}/thread-model/model-path-${TEST_CONFIG}.txt" model_path)
  foreach(thread_shared IN ITEMS OFF ON)
    set(model_build "${work}/thread-model-${thread_shared}")
    foreach(attempt RANGE 1 2)
      run_checked("configure nonempty pthread closure ${thread_shared}/${attempt}"
        "${CMAKE_COMMAND}" -S "${thread_source}" -B "${model_build}"
        -C "${TEST_INITIAL_CACHE}" ${generator_args}
        "-DTEST_THREAD_MODEL=${model_path}" "-DGKLIB_BUILD_SHARED_LIBS=${thread_shared}")
    endforeach()
    run_checked("build actual producer with nonempty closure"
      "${CMAKE_COMMAND}" --build "${model_build}"
      --config "${TEST_CONFIG}" --parallel 2)
    run_checked("run actual nonempty closure consumer"
      "${CMAKE_CTEST_COMMAND}" --test-dir "${model_build}"
      -C "${TEST_CONFIG}" --output-on-failure --no-tests=error
      -R "^thread-mask$")
    run_checked("run source-including and direct thread consumers"
      "${CMAKE_CTEST_COMMAND}" --test-dir "${model_build}/gklib"
      -C "${TEST_CONFIG}" --output-on-failure --no-tests=error
      -R "^GKlib\\.(pqueue-failure-recovery|memory-size-safety|recovery|cxx-consumer)$")
    set(model_prefix "${model_build}/prefix")
    run_checked("install actual nonempty closure producer"
      "${CMAKE_COMMAND}" --install "${model_build}"
      --config "${TEST_CONFIG}" --prefix "${model_prefix}")
    set(consumer_source "${model_build}/consumer-source")
    file(MAKE_DIRECTORY "${consumer_source}")
    file(COPY "${thread_source}/main.c" DESTINATION "${consumer_source}")
    configure_file("${thread_source}/main.c" "${consumer_source}/main.cpp" COPYONLY)
    file(CONFIGURE OUTPUT "${consumer_source}/CMakeLists.txt" CONTENT [=[
cmake_minimum_required(VERSION 3.24)
project(ThreadClosureConsumer C CXX)
find_package(GKlib CONFIG REQUIRED)
include("@MODULE_FILE@")
enable_testing()
foreach(language IN ITEMS c cpp)
  add_executable(consumer-${language} main.${language})
  target_link_libraries(consumer-${language} PRIVATE GKlib::GKlib)
  gklib_test_add_runtime(NAME consumer-${language} TARGET consumer-${language})
endforeach()
]=] @ONLY NEWLINE_STYLE LF)
    run_checked("configure installed nonempty closure C/C++ consumers"
      "${CMAKE_COMMAND}" -S "${consumer_source}" -B "${model_build}/consumer"
      -C "${TEST_INITIAL_CACHE}" ${generator_args}
      "-DCMAKE_PREFIX_PATH=${model_prefix}"
      "-DCMAKE_EXE_LINKER_FLAGS=-L\"${model_path}\"")
    run_checked("link installed nonempty closure C/C++ consumers"
      "${CMAKE_COMMAND}" --build "${model_build}/consumer"
      --config "${TEST_CONFIG}" --parallel 2)
    run_checked("run installed nonempty closure C/C++ consumers"
      "${CMAKE_CTEST_COMMAND}" --test-dir "${model_build}/consumer"
      -C "${TEST_CONFIG}" --output-on-failure)
  endforeach()

  # Replace an archive at the same path, then reconfigure an existing success.
  # Neither the unchanged flags nor a cached FindThreads success can bless it.
  run_checked("configure missing-mask model"
    "${CMAKE_COMMAND}" -S "${model_source}" -B "${work}/thread-model"
    -DTEST_EMPTY_MODEL=ON)
  run_checked("build missing-mask model"
    "${CMAKE_COMMAND}" --build "${work}/thread-model"
    --config "${TEST_CONFIG}" --parallel 2)
  execute_process(COMMAND "${CMAKE_COMMAND}" -S "${thread_source}"
    -B "${work}/thread-model-OFF"
    RESULT_VARIABLE stale_result OUTPUT_VARIABLE stale_output ERROR_VARIABLE stale_error)
  if(stale_result EQUAL 0 OR NOT "${stale_output}\n${stale_error}" MATCHES
      "GKlib recovery requires linkable POSIX pthread signal masks")
    message(FATAL_ERROR "An in-place dependency change reused a stale closure")
  endif()
  run_checked("restore nonempty model configuration"
    "${CMAKE_COMMAND}" -S "${model_source}" -B "${work}/thread-model"
    -DTEST_EMPTY_MODEL=OFF)
  run_checked("restore nonempty model archive"
    "${CMAKE_COMMAND}" --build "${work}/thread-model"
    --config "${TEST_CONFIG}" --parallel 2)
  run_checked("revalidate restored closure"
    "${CMAKE_COMMAND}" -S "${thread_source}" -B "${work}/thread-model-OFF")
  file(READ "${work}/wrap-build/wrap-supported.txt" thread_wrapping)
  if(thread_wrapping)
    execute_process(COMMAND "${CMAKE_COMMAND}"
      -S "${thread_source}" -B "${work}/thread-negative-build"
      -C "${TEST_INITIAL_CACHE}" ${generator_args} -DTEST_REJECT_MASK_LINK=ON
      RESULT_VARIABLE thread_result OUTPUT_VARIABLE thread_output
      ERROR_VARIABLE thread_error)
    if(thread_result EQUAL 0 OR
        NOT "${thread_output}\n${thread_error}" MATCHES
          "GKlib recovery requires linkable POSIX pthread signal masks")
      message(FATAL_ERROR
        "Unavailable signal masks were not rejected at configuration\n"
        "${thread_output}\n${thread_error}")
    endif()
  endif()
endif()
