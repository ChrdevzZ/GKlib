if(NOT DEFINED SPLATT2SVD OR NOT DEFINED WORK_DIR)
  message(FATAL_ERROR "SPLATT2SVD and WORK_DIR are required")
endif()

if(CROSSCOMPILING AND NOT EMULATOR)
  message("GKLIB_RUNTIME_SKIPPED: no cross-compiling emulator")
  return()
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")

function(run_case name input expected_result)
  set(path "${WORK_DIR}/${name}.tns")
  file(WRITE "${path}" "${input}")
  execute_process(
    COMMAND ${EMULATOR} "${SPLATT2SVD}" "${path}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(expected_result STREQUAL "success")
    if(NOT result EQUAL 0)
      message(FATAL_ERROR "${name}: expected success, got ${result}: ${error}")
    endif()
  elseif(result EQUAL 0)
    message(FATAL_ERROR "${name}: malformed input was accepted; output: ${output}")
  endif()
  set(${name}_OUTPUT "${output}" PARENT_SCOPE)
endfunction()

run_case(valid "1 1 1 2.5\n2 1 2 -1.0\n" success)
set(expected "2 2 2\n1\n0 2.500\n1\n1 -1.000\n")
if(NOT valid_OUTPUT STREQUAL expected)
  message(FATAL_ERROR
    "valid: output changed\nexpected=[${expected}]\nactual=[${valid_OUTPUT}]")
endif()

run_case(no_final_newline "1 1 1 2.5" success)
run_case(trailing_field "1 1 1 2.5 extra\n" failure)
run_case(zero_coordinate "0 1 1 2.5\n" failure)
run_case(large_coordinate "2147483648 1 1 2.5\n" failure)
run_case(product_overflow "2147483647 2147483647 1 2.5\n" failure)
run_case(nonfinite "1 1 1 nan\n" failure)
run_case(too_few_fields "1 1 1\n" failure)
run_case(empty "" failure)

set(growth_input "")
foreach(index RANGE 1 1025)
  string(APPEND growth_input "1 1 ${index} 1.0\n")
endforeach()
run_case(growth "${growth_input}" success)
if(NOT growth_OUTPUT MATCHES "^1 1025 1025\n")
  message(FATAL_ERROR "growth: unexpected header: ${growth_OUTPUT}")
endif()

execute_process(COMMAND ${EMULATOR} "${SPLATT2SVD}"
  RESULT_VARIABLE missing_result
  OUTPUT_QUIET ERROR_QUIET)
if(missing_result EQUAL 0)
  message(FATAL_ERROR "missing arguments returned success")
endif()
