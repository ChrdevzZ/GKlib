cmake_minimum_required(VERSION 3.24)
project(GKlibPCREPackageConsumer LANGUAGES CXX)

# The dependency module must restore CMake's caller-owned check state.
set(CMAKE_REQUIRED_FLAGS "PCRE_CHECK_STATE_SENTINEL")
find_package(GKlib CONFIG REQUIRED)
if(NOT CMAKE_REQUIRED_FLAGS STREQUAL "PCRE_CHECK_STATE_SENTINEL")
  message(FATAL_ERROR "FindPCRE leaked CMAKE_REQUIRED check state")
endif()
unset(CMAKE_REQUIRED_FLAGS)

# Package discovery must not enable C in this C++-only consumer.
get_property(_enabled_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
if("C" IN_LIST _enabled_languages)
  message(FATAL_ERROR "The C++ package consumer unexpectedly enabled C")
endif()

# GKlib's package config must recreate every dependency target it exports.
foreach(_pcre_target IN ITEMS PCRE::pcre PCRE::pcreposix PCRE::POSIX)
  if(NOT TARGET ${_pcre_target})
    message(FATAL_ERROR "GKlib did not restore dependency target ${_pcre_target}")
  endif()
endforeach()

# When requested, verify imported target types match the tested PCRE package.
if(DEFINED EXPECT_PCRE_LINKAGE)
  get_target_property(_pcre_type PCRE::pcre TYPE)
  get_target_property(_pcre_posix_type PCRE::pcreposix TYPE)
  if(EXPECT_PCRE_LINKAGE STREQUAL "STATIC")
    set(_expected_type STATIC_LIBRARY)
  elseif(EXPECT_PCRE_LINKAGE STREQUAL "SHARED")
    set(_expected_type SHARED_LIBRARY)
  else()
    message(FATAL_ERROR "EXPECT_PCRE_LINKAGE must be STATIC or SHARED")
  endif()
  if(NOT _pcre_type STREQUAL _expected_type OR
    NOT _pcre_posix_type STREQUAL _expected_type)
    message(FATAL_ERROR
      "Expected ${_expected_type}, got ${_pcre_type} and ${_pcre_posix_type}")
  endif()
endif()

# Exercise the transitive include and link interface from C++.
add_executable(gklib-pcre-package-consumer main.cpp)
target_compile_features(gklib-pcre-package-consumer PRIVATE cxx_std_11)
target_link_libraries(gklib-pcre-package-consumer PRIVATE GKlib::GKlib)

enable_testing()
add_test(NAME GKlib.pcre-package COMMAND gklib-pcre-package-consumer)

# Shared Windows consumers need both import libraries and runtime DLLs.
if(WIN32 AND EXPECT_PCRE_LINKAGE STREQUAL "SHARED")
  foreach(_pcre_target IN ITEMS PCRE::pcre PCRE::pcreposix)
    get_target_property(_pcre_dll ${_pcre_target} IMPORTED_LOCATION)
    get_target_property(_pcre_implib ${_pcre_target} IMPORTED_IMPLIB)
    if(NOT EXISTS "${_pcre_dll}" OR NOT EXISTS "${_pcre_implib}")
      message(FATAL_ERROR
        "${_pcre_target} must provide an import library and runtime DLL")
    endif()
  endforeach()
  add_custom_command(TARGET gklib-pcre-package-consumer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "$<TARGET_RUNTIME_DLLS:gklib-pcre-package-consumer>"
      "$<TARGET_FILE_DIR:gklib-pcre-package-consumer>"
    COMMAND_EXPAND_LISTS)
endif()
