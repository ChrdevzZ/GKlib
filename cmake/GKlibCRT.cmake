# Shared GKlib exposes FILE pointers and CRT signal state through its public
# interface. A separate static CRT in each DLL cannot preserve that contract.
# Check actual compiler flags, including an empty standard runtime policy;
# matching compiler names alone do not make these CRT objects interoperable.
include_guard(GLOBAL)


function(gklib_check_shared_crt target)
  get_target_property(type ${target} TYPE)
  if(NOT WIN32 OR NOT type STREQUAL "SHARED_LIBRARY" OR
      NOT (CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR
           CMAKE_C_SIMULATE_ID STREQUAL "MSVC"))
    return()
  endif()

  include(CheckCSourceCompiles)
  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  get_target_property(runtime ${target} MSVC_RUNTIME_LIBRARY)
  if(NOT runtime STREQUAL "runtime-NOTFOUND")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "${runtime}")
  endif()
  if(CMAKE_CONFIGURATION_TYPES)
    set(configs ${CMAKE_CONFIGURATION_TYPES})
    # A multi-config try-compile can build a custom configuration only when its
    # generated project receives the caller's complete configuration set.
    list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
      CMAKE_CONFIGURATION_TYPES)
    list(REMOVE_DUPLICATES CMAKE_TRY_COMPILE_PLATFORM_VARIABLES)
  elseif(CMAKE_BUILD_TYPE)
    set(configs "${CMAKE_BUILD_TYPE}")
  else()
    set(configs __DEFAULT)
  endif()
  list(REMOVE_DUPLICATES configs)
  file(SHA256 "${CMAKE_CURRENT_FUNCTION_LIST_FILE}" implementation)

  # These are compile-only checks. No target program runs during configuration,
  # and function scope restores the parent's compiler/check settings on return.
  foreach(config IN LISTS configs)
    if(config STREQUAL "__DEFAULT")
      set(config "")
    endif()
    string(TOUPPER "${config}" upper)
    set(CMAKE_TRY_COMPILE_CONFIGURATION "${config}")
    set(CMAKE_BUILD_TYPE "${config}")
    string(SHA256 signature
      "${implementation};${CMAKE_VERSION};${CMAKE_C_COMPILER};${CMAKE_C_COMPILER_VERSION};${CMAKE_C_COMPILER_TARGET};${CMAKE_C_FLAGS};${CMAKE_C_FLAGS_${upper}};${CMAKE_MSVC_RUNTIME_LIBRARY};${config};${CMAKE_CONFIGURATION_TYPES};${CMAKE_TOOLCHAIN_FILE};${CMAKE_GENERATOR_PLATFORM};${CMAKE_GENERATOR_TOOLSET};$ENV{CL};$ENV{_CL_};$ENV{INCLUDE}")
    set(cache "GKLIB_SHARED_CRT_${signature}")
    check_c_source_compiles("#if !defined(_DLL)
#error A shared CRT is required for GKlib DLL interfaces
#endif
int main(void) { return 0; }" ${cache})
    if(NOT ${cache})
      set(sanity_cache "GKLIB_SHARED_CRT_SANITY_${signature}")
      check_c_source_compiles("int main(void) { return 0; }" ${sanity_cache})
      if(${sanity_cache})
        message(FATAL_ERROR
          "Shared GKlib requires the DLL CRT (/MD or /MDd) in configuration '${config}': FILE pointers and signal state cross its DLL boundary. Use the DLL MSVC runtime, or build static GKlib with /MT or /MTd. See the shared CRT check in CMake's configure log.")
      else()
        message(FATAL_ERROR
          "Cannot validate the DLL CRT for shared GKlib in configuration '${config}'. The compile-only probe failed before evaluating its _DLL requirement. See the shared CRT check in CMake's configure log.")
      endif()
    endif()
  endforeach()
  cmake_pop_check_state()
endfunction()
