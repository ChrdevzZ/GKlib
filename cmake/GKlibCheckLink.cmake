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

  # A source check must use the same effective configuration as the target that
  # will consume its result. Multi-config generators retain their first configured
  # entry unless the caller selected a configuration explicitly.
  set(_gklib_link_check_config "${CMAKE_TRY_COMPILE_CONFIGURATION}")
  if(NOT _gklib_link_check_config)
    if(CMAKE_CONFIGURATION_TYPES)
      list(GET CMAKE_CONFIGURATION_TYPES 0 _gklib_link_check_config)
    else()
      set(_gklib_link_check_config "${CMAKE_BUILD_TYPE}")
    endif()
  endif()

  if(_gklib_link_check_config)
    set(CMAKE_TRY_COMPILE_CONFIGURATION "${_gklib_link_check_config}")
    string(TOUPPER "${_gklib_link_check_config}"
      _gklib_link_check_config_upper)

    # try_compile selects the requested configuration, but it does not forward
    # caller-defined configuration linker flags automatically. Pass both flag
    # families as platform inputs so the check matches the eventual link.
    list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
      "CMAKE_${_gklib_link_check_language}_FLAGS_${_gklib_link_check_config_upper}"
      "CMAKE_EXE_LINKER_FLAGS_${_gklib_link_check_config_upper}")
    if(CMAKE_CONFIGURATION_TYPES)
      list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
        CMAKE_CONFIGURATION_TYPES)
    endif()
    list(REMOVE_DUPLICATES CMAKE_TRY_COMPILE_PLATFORM_VARIABLES)
  endif()

  #-----------------------------------------------------------------------------
  # CACHE SIGNATURE
  #-----------------------------------------------------------------------------
  # CMake's built-in checks cache by variable name. Hash every relevant input so
  # changing toolchains, flags, search paths, or check state creates a fresh key.
  file(SHA256 "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
    _gklib_link_check_implementation)
  set(_gklib_link_check_signature
    "implementation=[${_gklib_link_check_implementation}]\nsource=[${source}]\nlanguage=[${_gklib_link_check_language}]\n")
  foreach(_gklib_link_check_name IN ITEMS
      CMAKE_COMMAND
      CMAKE_VERSION
      CMAKE_MAKE_PROGRAM
      CMAKE_${_gklib_link_check_language}_COMPILER
      CMAKE_${_gklib_link_check_language}_COMPILER_ARG1
      CMAKE_${_gklib_link_check_language}_COMPILER_ID
      CMAKE_${_gklib_link_check_language}_COMPILER_VERSION
      CMAKE_${_gklib_link_check_language}_COMPILER_FRONTEND_VARIANT
      CMAKE_${_gklib_link_check_language}_SIMULATE_ID
      CMAKE_${_gklib_link_check_language}_SIMULATE_VERSION
      CMAKE_${_gklib_link_check_language}_COMPILER_TARGET
      CMAKE_${_gklib_link_check_language}_COMPILER_ARCHITECTURE_ID
      CMAKE_${_gklib_link_check_language}_COMPILER_EXTERNAL_TOOLCHAIN
      CMAKE_${_gklib_link_check_language}_COMPILER_LAUNCHER
      CMAKE_${_gklib_link_check_language}_LINKER_LAUNCHER
      CMAKE_${_gklib_link_check_language}_COMPILER_LINKER
      CMAKE_${_gklib_link_check_language}_COMPILER_LINKER_ID
      CMAKE_${_gklib_link_check_language}_COMPILER_LINKER_VERSION
      CMAKE_${_gklib_link_check_language}_COMPILER_LINKER_FRONTEND_VARIANT
      CMAKE_${_gklib_link_check_language}_LINK_MODE
      CMAKE_${_gklib_link_check_language}_LINK_EXECUTABLE
      CMAKE_${_gklib_link_check_language}_LINKER_WRAPPER_FLAG
      CMAKE_${_gklib_link_check_language}_LINKER_WRAPPER_FLAG_SEP
      CMAKE_LINKER
      CMAKE_LINKER_LINK
      CMAKE_LINKER_LLD
      CMAKE_SYSROOT
      CMAKE_SYSROOT_COMPILE
      CMAKE_SYSROOT_LINK
      CMAKE_OSX_ARCHITECTURES
      CMAKE_GENERATOR
      CMAKE_GENERATOR_INSTANCE
      CMAKE_GENERATOR_PLATFORM
      CMAKE_GENERATOR_TOOLSET
      CMAKE_TOOLCHAIN_FILE
      CMAKE_SYSTEM_NAME
      CMAKE_SYSTEM_VERSION
      CMAKE_SYSTEM_PROCESSOR
      CMAKE_SIZEOF_VOID_P
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

  # CMake 3.29 and newer allow projects and toolchains to define custom linker
  # types. Hash every implementation variable, including the 3.29--3.31 MODE
  # input, so changing the command behind an unchanged type reruns the probe.
  get_cmake_property(_gklib_link_check_variables VARIABLES)
  list(SORT _gklib_link_check_variables)
  foreach(_gklib_link_check_name IN LISTS _gklib_link_check_variables)
    if(_gklib_link_check_name MATCHES
        "^CMAKE_${_gklib_link_check_language}_USING_LINKER_")
      string(APPEND _gklib_link_check_signature
        "${_gklib_link_check_name}=[${${_gklib_link_check_name}}]\n")
    endif()
  endforeach()

  if(_gklib_link_check_config)
    foreach(_gklib_link_check_prefix IN ITEMS
        CMAKE_${_gklib_link_check_language}_FLAGS
        CMAKE_EXE_LINKER_FLAGS)
      set(_gklib_link_check_name
        "${_gklib_link_check_prefix}_${_gklib_link_check_config_upper}")
      string(APPEND _gklib_link_check_signature
        "${_gklib_link_check_name}=[${${_gklib_link_check_name}}]\n")
    endforeach()
  endif()

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
  # A compiler that ignores a requested option has not proved the requested
  # link capability. Treat the common driver diagnostics as failed probes even
  # when the compiler exits successfully.
  set(_gklib_link_check_fail_patterns
    FAIL_REGEX "[Uu]nrecogni[sz]ed [^\n]*option"
    FAIL_REGEX "[Uu]nknown [^\n]*option"
    FAIL_REGEX "unknown argument ignored"
    FAIL_REGEX "argument unused"
    FAIL_REGEX "[Ii]gnoring unknown option"
    FAIL_REGEX "warning D9002"
    FAIL_REGEX "option[^\n]*not supported"
    FAIL_REGEX "invalid argument [^\n]*option")

  if(_gklib_link_check_language STREQUAL "C")
    check_c_source_compiles("${source}" ${_gklib_link_check_variable}
      ${_gklib_link_check_fail_patterns})
  elseif(_gklib_link_check_language STREQUAL "CXX")
    check_cxx_source_compiles("${source}" ${_gklib_link_check_variable}
      ${_gklib_link_check_fail_patterns})
  else()
    check_fortran_source_compiles("${source}" ${_gklib_link_check_variable}
      SRC_EXT F90 ${_gklib_link_check_fail_patterns})
  endif()
  set(${result} "${${_gklib_link_check_variable}}" PARENT_SCOPE)
  cmake_pop_check_state()
endfunction()
