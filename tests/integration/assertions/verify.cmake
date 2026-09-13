cmake_minimum_required(VERSION 3.24)

foreach(_required IN ITEMS DESCRIPTION EXECUTABLE ASSERTION_KIND EXPECT_TRIGGER)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

if(NOT DEFINED EXECUTION_TIMEOUT OR EXECUTION_TIMEOUT STREQUAL "")
  set(EXECUTION_TIMEOUT 10)
endif()

if(NOT ASSERTION_KIND MATCHES "^(ordinary|expensive)$")
  message(FATAL_ERROR "Unknown assertion kind: ${ASSERTION_KIND}")
endif()

get_filename_component(_run_directory "${EXECUTABLE}" DIRECTORY)
set(_command "${EXECUTABLE}")
if(EXECUTION_PREFIX)
  list(PREPEND _command ${EXECUTION_PREFIX})
endif()
execute_process(
  COMMAND ${_command} ${EXECUTABLE_ARGUMENTS}
  WORKING_DIRECTORY "${_run_directory}"
  TIMEOUT "${EXECUTION_TIMEOUT}"
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _stdout
  ERROR_VARIABLE _stderr)

set(_output "${_stdout}\n${_stderr}")
set(_marker "GKLIB_ASSERTION_ARMED:${ASSERTION_KIND}")
if(NOT _output MATCHES "(^|[\r\n])${_marker}([\r\n]|$)")
  message(FATAL_ERROR
    "${DESCRIPTION} did not emit the expected start marker\n${_output}")
endif()

if(EXPECT_TRIGGER)
  if(NOT "${_result}" STREQUAL "86")
    message(FATAL_ERROR
      "${DESCRIPTION} did not exit through the SIGABRT handler (result: ${_result})\n${_output}")
  endif()
  if(NOT _output MATCHES "[ *]ASSERTION failed")
    message(FATAL_ERROR
      "${DESCRIPTION} did not emit the GKlib assertion diagnostic\n${_output}")
  endif()
else()
  if(NOT "${_result}" STREQUAL "0")
    message(FATAL_ERROR
      "${DESCRIPTION} unexpectedly failed (result: ${_result})\n${_output}")
  endif()
  if(_output MATCHES "[ *]ASSERTION failed")
    message(FATAL_ERROR
      "${DESCRIPTION} emitted an assertion diagnostic while disabled\n${_output}")
  endif()
endif()
