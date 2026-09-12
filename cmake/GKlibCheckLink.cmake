include_guard(GLOBAL)


include(CMakePushCheckState)


# Compile a complete executable so a cached success proves both compiler and
# linker support. Package probes may use an already enabled CXX or Fortran
# compiler; this helper never enables an additional project language.
function(gklib_check_link source result)
  set(_gklib_link_check_language C)
  if(ARGC GREATER 2)
    if(NOT ARGC EQUAL 4 OR NOT ARGV2 STREQUAL "LANGUAGE" OR
      NOT ARGV3 MATCHES "^(C|CXX|Fortran)$")
      message(FATAL_ERROR
        "gklib_check_link expects SOURCE RESULT [LANGUAGE C|CXX|Fortran]")
    endif()
    set(_gklib_link_check_language "${ARGV3}")
  endif()

  if(_gklib_link_check_language STREQUAL "C")
    include(CheckCSourceCompiles)
  elseif(_gklib_link_check_language STREQUAL "CXX")
    include(CheckCXXSourceCompiles)
  else()
    include(CheckFortranSourceCompiles)
  endif()

  cmake_push_check_state()
  set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

  #-----------------------------------------------------------------------------
  # CACHE SIGNATURE
  #-----------------------------------------------------------------------------
  # CMake's built-in checks cache by variable name. Hash every relevant input so
  # changing toolchains, flags, search paths, or check state creates a fresh key.
  set(_gklib_link_check_signature
    "source=[${source}]\nlanguage=[${_gklib_link_check_language}]\n")
  foreach(_gklib_link_check_name IN ITEMS
      CMAKE_${_gklib_link_check_language}_COMPILER
      CMAKE_${_gklib_link_check_language}_COMPILER_ARG1
      CMAKE_${_gklib_link_check_language}_COMPILER_ID
      CMAKE_${_gklib_link_check_language}_COMPILER_VERSION
      CMAKE_${_gklib_link_check_language}_COMPILER_FRONTEND_VARIANT
      CMAKE_${_gklib_link_check_language}_SIMULATE_ID
      CMAKE_${_gklib_link_check_language}_SIMULATE_VERSION
      CMAKE_${_gklib_link_check_language}_COMPILER_TARGET
      CMAKE_${_gklib_link_check_language}_COMPILER_EXTERNAL_TOOLCHAIN
      CMAKE_${_gklib_link_check_language}_COMPILER_LAUNCHER
      CMAKE_${_gklib_link_check_language}_LINKER_LAUNCHER
      CMAKE_SYSROOT
      CMAKE_SYSROOT_COMPILE
      CMAKE_SYSROOT_LINK
      CMAKE_OSX_ARCHITECTURES
      CMAKE_GENERATOR
      CMAKE_GENERATOR_PLATFORM
      CMAKE_GENERATOR_TOOLSET
      CMAKE_BUILD_TYPE
      CMAKE_CONFIGURATION_TYPES
      CMAKE_TRY_COMPILE_CONFIGURATION
      CMAKE_TRY_COMPILE_NO_PLATFORM_VARIABLES
      CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
      CMAKE_MSVC_RUNTIME_LIBRARY
      CMAKE_LINKER_TYPE
      CMAKE_${_gklib_link_check_language}_LINKER_TYPE
      CMAKE_${_gklib_link_check_language}_STANDARD_LIBRARIES
      CMAKE_${_gklib_link_check_language}_FLAGS
      CMAKE_EXE_LINKER_FLAGS
      CMAKE_REQUIRED_FLAGS
      CMAKE_REQUIRED_DEFINITIONS
      CMAKE_REQUIRED_INCLUDES
      CMAKE_REQUIRED_LINK_OPTIONS
      CMAKE_REQUIRED_LIBRARIES
      CMAKE_REQUIRED_LINK_DIRECTORIES)
    string(APPEND _gklib_link_check_signature
      "${_gklib_link_check_name}=[${${_gklib_link_check_name}}]\n")
  endforeach()

  set(_gklib_link_check_configs
    Debug Release RelWithDebInfo MinSizeRel
    ${CMAKE_CONFIGURATION_TYPES}
    "${CMAKE_BUILD_TYPE}"
    "${CMAKE_TRY_COMPILE_CONFIGURATION}")
  list(REMOVE_DUPLICATES _gklib_link_check_configs)

  foreach(_gklib_link_check_config IN LISTS _gklib_link_check_configs)
    if(_gklib_link_check_config)
      string(TOUPPER "${_gklib_link_check_config}"
        _gklib_link_check_config_upper)
      foreach(_gklib_link_check_prefix IN ITEMS
          CMAKE_${_gklib_link_check_language}_FLAGS
          CMAKE_EXE_LINKER_FLAGS)
        set(_gklib_link_check_name
          "${_gklib_link_check_prefix}_${_gklib_link_check_config_upper}")
        string(APPEND _gklib_link_check_signature
          "${_gklib_link_check_name}=[${${_gklib_link_check_name}}]\n")
      endforeach()
    endif()
  endforeach()

  foreach(_gklib_link_check_name IN LISTS
      CMAKE_TRY_COMPILE_PLATFORM_VARIABLES)
    string(APPEND _gklib_link_check_signature
      "platform:${_gklib_link_check_name}=[${${_gklib_link_check_name}}]\n")
  endforeach()

  # Compiler search paths and implicit driver options affect link capability.
  foreach(_gklib_link_check_name IN ITEMS
      PATH LIB LIBPATH INCLUDE LIBRARY_PATH COMPILER_PATH CPATH
      C_INCLUDE_PATH CPLUS_INCLUDE_PATH SDKROOT CL _CL_ LINK _LINK_)
    string(APPEND _gklib_link_check_signature
      "env:${_gklib_link_check_name}=[$ENV{${_gklib_link_check_name}}]\n")
  endforeach()
  string(SHA256 _gklib_link_check_id "${_gklib_link_check_signature}")
  set(_gklib_link_check_variable
    "GKLIB_LINK_CHECK_${_gklib_link_check_id}")

  #-----------------------------------------------------------------------------
  # COMPILE AND LINK
  #-----------------------------------------------------------------------------
  if(_gklib_link_check_language STREQUAL "C")
    check_c_source_compiles("${source}" ${_gklib_link_check_variable})
  elseif(_gklib_link_check_language STREQUAL "CXX")
    check_cxx_source_compiles("${source}" ${_gklib_link_check_variable})
  else()
    check_fortran_source_compiles("${source}" ${_gklib_link_check_variable}
      SRC_EXT F90)
  endif()
  set(${result} "${${_gklib_link_check_variable}}" PARENT_SCOPE)
  cmake_pop_check_state()
endfunction()
