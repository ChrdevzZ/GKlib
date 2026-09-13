include("${PROJECT_SOURCE_DIR}/cmake/GKlibIPO.cmake")
include(GenerateExportHeader)

#-------------------------------------------------------------------------------
# LIBRARY TARGET
#-------------------------------------------------------------------------------
if(GKLIB_BUILD_SHARED_LIBS)
  add_library(GKlib SHARED)
else()
  add_library(GKlib STATIC)
endif()
add_library(GKlib::GKlib ALIAS GKlib)

# FILE and signal APIs require a single shared CRT across Windows DLL users.
include("${PROJECT_SOURCE_DIR}/cmake/GKlibCRT.cmake")
gklib_check_shared_crt(GKlib)

if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "IntelLLVM")
  intel_runtime_link(GKlib)
endif()

# Public headers are listed as private sources so IDEs show the complete API
# without changing how consumers receive it.
target_sources(GKlib PRIVATE
  b64.c
  blas.c
  cache.c
  csr.c
  error.c
  evaluate.c
  fkvkselect.c
  fs.c
  getopt.c
  gk_util.c
  gkregex.c
  graph.c
  htable.c
  io.c
  itemsets.c
  mcore.c
  memory.c
  memory_internal.h
  pqueue.c
  random.c
  rw.c
  seq.c
  sort.c
  string.c
  timers.c
  tokenizer.c
  ${GKLIB_PUBLIC_HEADERS})

set_target_properties(GKlib PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED YES
  C_EXTENSIONS YES
  SOVERSION ${PROJECT_VERSION_MAJOR}
  VERSION ${PROJECT_VERSION})

if(DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
  set_property(TARGET GKlib PROPERTY POSITION_INDEPENDENT_CODE
    "${CMAKE_POSITION_INDEPENDENT_CODE}")
else()
  set_property(TARGET GKlib PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

if(MINGW)
  target_link_options(GKlib PRIVATE
    -Wl,--export-all-symbols,--exclude-symbols,gk_realloc_mcore)
endif()

#-------------------------------------------------------------------------------
# PUBLIC INTERFACE
#-------------------------------------------------------------------------------
generate_export_header(GKlib
  EXPORT_FILE_NAME "${GKLIB_GENERATED_INCLUDE_DIR}/gklib_export.h"
  EXPORT_MACRO_NAME GKLIB_EXPORT
  STATIC_DEFINE GKLIB_STATIC_DEFINE)

target_include_directories(GKlib
  PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${GKLIB_GENERATED_INCLUDE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

#-------------------------------------------------------------------------------
# ASSERTION AND DEBUG POLICIES
#-------------------------------------------------------------------------------
# AUTO follows each active configuration so multi-config generators retain
# distinct Debug and non-Debug behavior from one configured build tree.
if(GKLIB_ASSERTIONS STREQUAL "AUTO")
  set(_gklib_assertions_value "$<IF:$<CONFIG:Debug>,1,0>")
elseif(GKLIB_ASSERTIONS STREQUAL "ON")
  set(_gklib_assertions_value 1)
else()
  set(_gklib_assertions_value 0)
endif()

if(GKLIB_DEBUG STREQUAL "AUTO")
  set(_gklib_debug_value "$<IF:$<CONFIG:Debug>,1,0>")
elseif(GKLIB_DEBUG STREQUAL "ON")
  set(_gklib_debug_value 1)
else()
  set(_gklib_debug_value 0)
endif()

# The enabled-state definitions are public API inputs used by GKlib headers;
# implementation-only NDEBUG/DEBUG definitions remain private to the library.
target_compile_definitions(GKlib
  PUBLIC
    "GKLIB_ASSERTIONS_ENABLED=${_gklib_assertions_value}"
    "GKLIB_ASSERTIONS_EXPENSIVE_ENABLED=$<BOOL:${GKLIB_ASSERTIONS_EXPENSIVE}>"
    "$<$<NOT:$<BOOL:${GKLIB_BUILD_SHARED_LIBS}>>:GKLIB_STATIC_DEFINE>"
  PRIVATE
    "$<$<NOT:$<BOOL:${_gklib_assertions_value}>>:NDEBUG>"
    "$<$<NOT:$<BOOL:${GKLIB_ASSERTIONS_EXPENSIVE}>>:NDEBUG2>"
    "$<$<BOOL:${_gklib_debug_value}>:DEBUG>"
    "$<$<PLATFORM_ID:Linux>:_POSIX_C_SOURCE=200809L>")

if(GKLIB_ASSERTIONS STREQUAL "ON")
  if(MSVC)
    target_compile_options(GKlib PRIVATE /UNDEBUG)
  else()
    target_compile_options(GKlib PRIVATE -UNDEBUG)
  endif()
endif()

#-------------------------------------------------------------------------------
# OPTIMIZATION AND INSTRUMENTATION
#-------------------------------------------------------------------------------
if(GKLIB_NATIVE_OPTIMIZATION)
  if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "GKLIB_NATIVE_OPTIMIZATION is unavailable when cross-compiling")
  endif()
  if(CMAKE_OSX_ARCHITECTURES)
    list(LENGTH CMAKE_OSX_ARCHITECTURES _gklib_osx_arch_count)
    if(_gklib_osx_arch_count GREATER 1)
      message(FATAL_ERROR
        "GKLIB_NATIVE_OPTIMIZATION is unavailable for multi-architecture builds")
    endif()
  endif()
  if(MSVC)
    message(FATAL_ERROR
      "GKLIB_NATIVE_OPTIMIZATION is unavailable with the MSVC-compatible driver")
  else()
    check_c_compiler_flag("-march=native" GKLIB_HAVE_MARCH_NATIVE)
    if(NOT GKLIB_HAVE_MARCH_NATIVE)
      message(FATAL_ERROR
        "GKLIB_NATIVE_OPTIMIZATION was requested, but -march=native is unavailable")
    endif()
    target_compile_options(GKlib PRIVATE -march=native)
  endif()
endif()

if(GKLIB_GPROF)
  # MSVC-style drivers may ignore -pg with a warning. Reject that unsupported
  # interface before a successful but uninstrumented executable can pass.
  if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR
      CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR
      "GKLIB_GPROF requires a driver supporting -pg; the MSVC frontend does not")
  endif()
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_FLAGS "-pg")
  set(CMAKE_REQUIRED_LINK_OPTIONS "-pg")
  gklib_check_link("int main(void) { return 0; }" GKLIB_HAVE_GPROF_RUNTIME)
  cmake_pop_check_state()
  if(NOT GKLIB_HAVE_GPROF_RUNTIME)
    message(FATAL_ERROR "GKLIB_GPROF was requested, but -pg is unavailable")
  endif()
  target_compile_options(GKlib PRIVATE -pg)
  target_link_options(GKlib PUBLIC -pg)
endif()

if(GKLIB_SANITIZERS)
  string(REPLACE "," ";" _gklib_sanitizer_list "${GKLIB_SANITIZERS}")
  foreach(_gklib_sanitizer IN LISTS _gklib_sanitizer_list)
    if(NOT _gklib_sanitizer MATCHES "^(address|undefined)$")
      message(FATAL_ERROR
        "Unsupported GKLIB_SANITIZERS entry '${_gklib_sanitizer}'; allowed values are address and undefined")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES _gklib_sanitizer_list)
  list(JOIN _gklib_sanitizer_list "," _gklib_sanitizers)
  if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    if("undefined" IN_LIST _gklib_sanitizer_list)
      message(FATAL_ERROR
        "The MSVC compiler supports only GKLIB_SANITIZERS=address")
    endif()
    set(_gklib_sanitizer_flag "/fsanitize=address")
    set(_gklib_sanitizer_link_flag "LINKER:/INFERASANLIBS")
  elseif(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang|IntelLLVM)$")
    set(_gklib_sanitizer_flag "-fsanitize=${_gklib_sanitizers}")
    set(_gklib_sanitizer_link_flag "${_gklib_sanitizer_flag}")
  else()
    message(FATAL_ERROR
      "GKLIB_SANITIZERS requires a GCC-, Clang-, or MSVC-compatible driver")
  endif()

  string(MAKE_C_IDENTIFIER "${_gklib_sanitizers}" _gklib_sanitizer_id)
  set(_gklib_sanitizer_check
    "GKLIB_HAVE_SANITIZER_RUNTIME_${_gklib_sanitizer_id}")
  set(_gklib_sanitizer_runtime "")
  if(WIN32 AND CMAKE_C_SIMULATE_ID STREQUAL "MSVC" AND
      CMAKE_C_COMPILER_ID MATCHES "Clang|IntelLLVM")
    # MSVC-ABI Clang-family objects need the producer's ASan libraries even
    # when CMake bypasses the compiler driver for the final link.
    set(_gklib_sanitizer_link_flag "")
    if("address" IN_LIST _gklib_sanitizer_list)
      include("${PROJECT_SOURCE_DIR}/cmake/WindowsASan.cmake")
      windows_asan_attach(GKlib GKlib gklib_check_link)
      set(_gklib_sanitizer_runtime GKlib::ASanRuntime)
    endif()
  endif()
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_FLAGS "${_gklib_sanitizer_flag}")
  set(CMAKE_REQUIRED_LINK_OPTIONS "${_gklib_sanitizer_link_flag}")
  set(CMAKE_REQUIRED_LIBRARIES "${_gklib_sanitizer_runtime}")

  # Do not confuse a silently ignored switch with usable instrumentation.
  # Memory/arithmetic operations additionally require the final-link runtime.
  set(_gklib_sanitizer_preamble
    "#ifndef __has_feature\n#define __has_feature(x) 0\n#endif\n")
  if("address" IN_LIST _gklib_sanitizer_list)
    string(APPEND _gklib_sanitizer_preamble
      "#if !defined(__SANITIZE_ADDRESS__) && !__has_feature(address_sanitizer)\n#error AddressSanitizer instrumentation is not enabled\n#endif\n")
  endif()
  if("undefined" IN_LIST _gklib_sanitizer_list AND
      CMAKE_C_COMPILER_ID MATCHES "Clang|IntelLLVM")
    string(APPEND _gklib_sanitizer_preamble
      "#if !__has_feature(undefined_behavior_sanitizer)\n#error UndefinedBehaviorSanitizer instrumentation is not enabled\n#endif\n")
  endif()

  gklib_check_link(
    "${_gklib_sanitizer_preamble}int main(int argc, char **argv) { volatile int values[2] = {0, 1}; int index = argc * argc; (void)argv; return values[index]; }"
    ${_gklib_sanitizer_check})
  cmake_pop_check_state()
  if(NOT ${_gklib_sanitizer_check})
    message(FATAL_ERROR
      "The compiler and linker do not support GKLIB_SANITIZERS=${GKLIB_SANITIZERS}")
  endif()

  target_compile_options(GKlib PRIVATE "${_gklib_sanitizer_flag}")
  if(NOT MSVC AND NOT CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(GKlib PRIVATE -fno-omit-frame-pointer)
  endif()
  # Consumers of static objects need the runtime selected by the producer.
  if(_gklib_sanitizer_link_flag)
    # Native MSVC resolves ASan imports in the DLL's own link. Do not force
    # its development-library inference onto an ordinary DLL consumer.
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" AND GKLIB_BUILD_SHARED_LIBS)
      target_link_options(GKlib PRIVATE "${_gklib_sanitizer_link_flag}")
    else()
      target_link_options(GKlib PUBLIC "${_gklib_sanitizer_link_flag}")
    endif()
  endif()
endif()

if(GKLIB_WARNINGS_AS_ERRORS)
  if(MSVC)
    target_compile_options(GKlib PRIVATE /WX)
  else()
    target_compile_options(GKlib PRIVATE -Werror)
  endif()
endif()

#-------------------------------------------------------------------------------
# LINK INTERFACE
#-------------------------------------------------------------------------------
# Math and regex are public because inline/API behavior can require them;
# execinfo is used only by GKlib's implementation.
if(GKLIB_MATH_LIBRARY)
  target_link_libraries(GKlib PUBLIC "${GKLIB_MATH_LIBRARY}")
endif()
if(GKLIB_EXECINFO_LIBRARY)
  target_link_libraries(GKlib PRIVATE "${GKLIB_EXECINFO_LIBRARY}")
endif()
if(GKLIB_REGEX_LINK_TARGET)
  target_link_libraries(GKlib PUBLIC "${GKLIB_REGEX_LINK_TARGET}")
endif()

# Discover the C runtime in an executable check, then export that dependency
# requirement so installed package consumers can reconstruct the link interface.
if(GKLIB_OPENMP)
  include("${PROJECT_SOURCE_DIR}/cmake/GKlibOpenMP.cmake")
  gklib_openmp_configure(GKlib)
  if(NOT GKLIB_BUILD_SHARED_LIBS)
    set(GKLIB_CONFIG_NEEDS_OPENMP ON PARENT_SCOPE)
  endif()
endif()

# Resolve IPO after instrumentation flags have been established.
gklib_target_ipo(GKlib)

#-------------------------------------------------------------------------------
# INSTALLATION
#-------------------------------------------------------------------------------
# Runtime artifacts and development import/archive data have separate component
# ownership so package managers can split deployed and build-time files.
if(GKLIB_INSTALL)
  install(TARGETS GKlib EXPORT GKlibTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT GKlib_Runtime
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT GKlib_Runtime
      NAMELINK_COMPONENT GKlib_Development
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT GKlib_Development
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endif()
