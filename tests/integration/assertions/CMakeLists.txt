cmake_minimum_required(VERSION 3.24)
project(GKlibAssertionConsumer LANGUAGES C)

if(NOT DEFINED GKLIB_SOURCE_DIR)
  message(FATAL_ERROR "GKLIB_SOURCE_DIR is required")
endif()

# Keep the fixture focused on assertion definitions from a static subproject.
set(GKLIB_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(GKLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(GKLIB_INSTALL OFF CACHE BOOL "" FORCE)
set(GKLIB_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory("${GKLIB_SOURCE_DIR}" gklib)

# Each executable tests one public assertion tier under consumer-side NDEBUG.
foreach(_kind IN ITEMS ordinary expensive)
  add_executable("assert-${_kind}" assertion_consumer.c)
  target_link_libraries("assert-${_kind}" PRIVATE GKlib::GKlib)
  target_compile_definitions("assert-${_kind}" PRIVATE
    NDEBUG
    "TEST_EXPENSIVE=$<STREQUAL:${_kind},expensive>")
  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/assert-${_kind}-$<CONFIG>.path"
    CONTENT "$<TARGET_FILE:assert-${_kind}>")
endforeach()
