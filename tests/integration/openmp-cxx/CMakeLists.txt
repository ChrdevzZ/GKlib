cmake_minimum_required(VERSION 3.24)
# Enable only the consumer language; package loading must not add C/CXX to a
# Fortran-only parent. CXX remains the default for the existing fixture.
set(GKLIB_CONSUMER_LANGUAGE CXX CACHE STRING "Consumer language: CXX or Fortran")
project(GKlibOpenMPConsumer LANGUAGES ${GKLIB_CONSUMER_LANGUAGE})

get_property(_initial_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
list(FIND _initial_languages "${GKLIB_CONSUMER_LANGUAGE}" _language_index)
if(_language_index EQUAL -1)
  message(FATAL_ERROR
    "The consumer language was not enabled: ${_initial_languages}")
endif()
if(GKLIB_CONSUMER_LANGUAGE STREQUAL "Fortran")
  foreach(_unexpected_language IN ITEMS C CXX)
    if(_unexpected_language IN_LIST _initial_languages)
      message(FATAL_ERROR
        "The test cache enabled ${_unexpected_language} in a Fortran-only consumer")
    endif()
  endforeach()
endif()

# Hide development libraries after compiler detection. Static packages must
# diagnose their missing runtime; shared packages need no OpenMP SDK to link.
if(CONSUMER_HIDE_RUNTIME)
  set(CMAKE_FIND_ROOT_PATH "${CMAKE_CURRENT_BINARY_DIR}/absent-sdk")
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
endif()

if(CONSUMER_RUNTIME_ROOT)
  # Only the staged runtime SDK may satisfy package discovery. Compiler
  # detection already finished; the subsequent build retains its normal SDK.
  set(OpenMP_ROOT "${CONSUMER_RUNTIME_ROOT}")
  set(CMAKE_C_IMPLICIT_LINK_DIRECTORIES "")
  set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")
  set(CMAKE_Fortran_IMPLICIT_LINK_DIRECTORIES "")
  set(ENV{LIB} "")
  set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH OFF)
  set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH OFF)
endif()

# Dependency probes must leave caller-owned check state and languages alone.
set(CMAKE_REQUIRED_FLAGS "GKLIB_CONSUMER_CHECK_STATE")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
get_property(_original_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
find_package(GKlib CONFIG REQUIRED)
if(NOT CMAKE_REQUIRED_FLAGS STREQUAL "GKLIB_CONSUMER_CHECK_STATE" OR
    NOT CMAKE_TRY_COMPILE_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
  message(FATAL_ERROR "GKlib changed consumer check state")
endif()
unset(CMAKE_REQUIRED_FLAGS)
get_property(_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
if(NOT _languages STREQUAL _original_languages)
  message(FATAL_ERROR "GKlib enabled an additional consumer language: ${_languages}")
endif()

# Execute a normalization region through the public C interface.
if(GKLIB_CONSUMER_LANGUAGE STREQUAL "Fortran")
  add_executable(gklib-openmp-cxx-consumer main.f90)
else()
  add_executable(gklib-openmp-cxx-consumer main.cpp)
  target_compile_features(gklib-openmp-cxx-consumer PRIVATE cxx_std_11)
endif()
target_link_libraries(gklib-openmp-cxx-consumer PRIVATE GKlib::GKlib)
if(WIN32)
  add_custom_command(TARGET gklib-openmp-cxx-consumer POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DLIBRARIES=$<TARGET_RUNTIME_DLLS:gklib-openmp-cxx-consumer>"
      "-DDESTINATION=$<TARGET_FILE_DIR:gklib-openmp-cxx-consumer>"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/CopyRuntime.cmake"
    VERBATIM)
endif()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/consumer-$<CONFIG>.path"
  CONTENT "$<TARGET_FILE:gklib-openmp-cxx-consumer>")

enable_testing()
add_test(NAME GKlib.optional-runtime-consumer COMMAND gklib-openmp-cxx-consumer)
