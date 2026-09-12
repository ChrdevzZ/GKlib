# Preserve the producer's OpenMP runtime without enabling languages or applying
# another compiler's OpenMP flags in installed-package consumers.
include_guard(GLOBAL)


function(gklib_openmp_configure target)
  set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
  find_package(OpenMP REQUIRED COMPONENTS C)
  target_link_libraries(${target} PRIVATE
    "$<BUILD_INTERFACE:OpenMP::OpenMP_C>"
    "$<INSTALL_INTERFACE:GKlib::OpenMPRuntime>")

  # Shared objects have already resolved this private dependency. Their DLLs
  # still need the OpenMP runtime at execution time, not its development SDK.
  if(GKLIB_BUILD_SHARED_LIBS OR NOT GKLIB_INSTALL)
    return()
  endif()

  set(configs ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
  if(NOT configs)
    set(configs NOCONFIG)
  endif()
  list(REMOVE_DUPLICATES configs)
  foreach(config IN LISTS configs)
    string(TOUPPER "${config}" upper)
    set(names "")
    foreach(library IN LISTS OpenMP_C_LIBRARIES)
      # Record filenames, never paths from the producer's SDK. Exact names
      # distinguish import archives from static runtimes on MinGW platforms.
      get_filename_component(name "${library}" NAME)
      list(APPEND names "${name}")
    endforeach()

    if(NOT names AND CMAKE_C_COMPILER_ID STREQUAL "MSVC")
      # MSVC's omp.h emits a default-library directive instead of reporting a
      # library through FindOpenMP. Probe the effective CRT, including explicit
      # flags and empty CMAKE_MSVC_RUNTIME_LIBRARY policies, for each config.
      set(CMAKE_TRY_COMPILE_CONFIGURATION "${config}")
      set(CMAKE_BUILD_TYPE "${config}")
      get_target_property(runtime ${target} MSVC_RUNTIME_LIBRARY)
      if(NOT runtime STREQUAL "runtime-NOTFOUND")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "${runtime}")
      endif()
      gklib_check_link("#ifndef _DEBUG\n#error Release CRT\n#endif\nint main(void) { return 0; }"
        debug_crt)
      # /openmp:llvm emits LIBOMP instead of VCOMP, even though FindOpenMP
      # reports neither default-library directive in its library list.
      if(OpenMP_C_FLAGS MATCHES "[-/]openmp:llvm($|[ ;])")
        if(debug_crt)
          set(names libompd.lib)
        else()
          set(names libomp.lib)
        endif()
      elseif(debug_crt)
        set(names vcompd.lib)
      else()
        set(names vcomp.lib)
      endif()
    elseif(NOT names AND WIN32 AND CMAKE_C_COMPILER_ID MATCHES "^Intel(LLVM)?$" AND
        OpenMP_C_FLAGS MATCHES "[-/]Qi?openmp($|[ ;])")
      # Intel's Windows driver also uses an object default-library directive.
      # Its supported OpenMP runtime is the DLL import library for every CRT;
      # unlike the Linux driver, Windows does not offer a static runtime mode.
      set(names libiomp5md.lib)
    endif()
    if(NOT names)
      message(FATAL_ERROR
        "FindOpenMP did not identify the C runtime libraries. Set OpenMP_C_LIB_NAMES and the corresponding OpenMP_<name>_LIBRARY entries before exporting static GKlib.")
    endif()
    set_property(TARGET ${target} PROPERTY _GKLIB_OPENMP_NAMES_${upper} "${names}")
  endforeach()
  set_property(TARGET ${target} PROPERTY _GKLIB_OPENMP_CONFIGS "${configs}")
endfunction()


function(gklib_openmp_install destination)
  get_target_property(configs GKlib _GKLIB_OPENMP_CONFIGS)
  foreach(config IN LISTS configs)
    string(TOUPPER "${config}" upper)
    get_target_property(names GKlib _GKLIB_OPENMP_NAMES_${upper})
    set(record "${PROJECT_BINARY_DIR}/cmake/GKlibOpenMP-${upper}.cmake")
    file(WRITE "${record}"
      "# Runtime filenames of the ${config} producer; no machine paths.\nset(_gklib_openmp_names_${upper} \"${names}\")\n")
    if(config STREQUAL "NOCONFIG")
      install(FILES "${record}" DESTINATION "${destination}"
        COMPONENT GKlib_Development)
    else()
      install(FILES "${record}" DESTINATION "${destination}"
        CONFIGURATIONS "${config}" COMPONENT GKlib_Development)
    endif()
  endforeach()
  install(FILES "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
    DESTINATION "${destination}" COMPONENT GKlib_Development)
endfunction()


function(gklib_openmp_import target)
  if(TARGET GKlib::OpenMPRuntime)
    return()
  endif()

  # Search only consumer-side hints/defaults. Fortran-only projects provide
  # their own implicit link directories; no C/CXX compiler is requested here.
  set(hints ${OpenMP_ROOT} ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES}
    ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES}
    ${CMAKE_Fortran_IMPLICIT_LINK_DIRECTORIES})
  cmake_path(CONVERT "$ENV{LIB}" TO_CMAKE_PATH_LIST environment_libs)
  list(APPEND hints ${environment_libs})
  get_target_property(configs ${target} IMPORTED_CONFIGURATIONS)
  foreach(config IN LISTS configs)
    set(record "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/GKlibOpenMP-${config}.cmake")
    if(NOT EXISTS "${record}")
      set(GKlib_FOUND FALSE PARENT_SCOPE)
      set(GKlib_NOT_FOUND_MESSAGE "Missing OpenMP runtime record: ${record}" PARENT_SCOPE)
      return()
    endif()
    include("${record}")
    set(libraries "")
    foreach(name IN LISTS _gklib_openmp_names_${config})
      unset(library)
      find_library(library NAMES "${name}" HINTS ${hints}
        PATH_SUFFIXES lib lib64 NO_CACHE)
      if(NOT library)
        set(GKlib_FOUND FALSE PARENT_SCOPE)
        set(GKlib_NOT_FOUND_MESSAGE
          "GKlib ${config} requires producer OpenMP runtime ${name}; set OpenMP_ROOT or CMAKE_PREFIX_PATH to a compatible SDK." PARENT_SCOPE)
        return()
      endif()
      list(APPEND libraries "${library}")
    endforeach()
    if(NOT libraries)
      set(GKlib_FOUND FALSE PARENT_SCOPE)
      set(GKlib_NOT_FOUND_MESSAGE "Empty OpenMP runtime record: ${record}" PARENT_SCOPE)
      return()
    endif()
    set(resolved_${config} "${libraries}")
  endforeach()

  # One imported library preserves CMake's normal configuration mapping. The
  # remaining runtime archives follow it in producer link order.
  add_library(GKlib::OpenMPRuntime UNKNOWN IMPORTED GLOBAL)
  set_property(TARGET GKlib::OpenMPRuntime PROPERTY IMPORTED_CONFIGURATIONS "${configs}")
  foreach(config IN LISTS configs)
    set(libraries "${resolved_${config}}")
    list(POP_FRONT libraries first)
    set_target_properties(GKlib::OpenMPRuntime PROPERTIES
      IMPORTED_LOCATION_${config} "${first}"
      IMPORTED_LINK_INTERFACE_LIBRARIES_${config} "${libraries}")
  endforeach()
endfunction()
