cmake_minimum_required(VERSION 3.24)
project(GKlibStaticDirectConsumer LANGUAGES C)

if(NOT DEFINED GKLIB_PREFIX)
  message(FATAL_ERROR "GKLIB_PREFIX is required")
endif()

# Locate the installed archive without importing GKlib's package metadata.
find_library(GKLIB_LIBRARY NAMES GKlib libGKlib
  PATHS "${GKLIB_PREFIX}/lib" "${GKLIB_PREFIX}/lib64"
  NO_DEFAULT_PATH REQUIRED)

# This consumer checks the declarations available to direct static users.
add_executable(gklib-static-direct-consumer main.c)
target_include_directories(gklib-static-direct-consumer PRIVATE
  "${GKLIB_PREFIX}/include")
target_link_libraries(gklib-static-direct-consumer PRIVATE "${GKLIB_LIBRARY}")
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/consumer-$<CONFIG>.path"
  CONTENT "$<TARGET_FILE:gklib-static-direct-consumer>")
