# Reuse an existing dependency graph when a parent already provides PCRE.
if(TARGET PCRE::POSIX)
  # Preserve explicit producer metadata. Otherwise infer only from a concrete
  # target whose CMake type states the linkage; an arbitrary interface graph
  # can contain either static or shared libraries and is not evidence enough.
  if(NOT PCRE_LINKAGE MATCHES "^(STATIC|SHARED)$")
    foreach(_pcre_existing_target IN ITEMS PCRE::POSIX PCRE::pcreposix)
      if(TARGET ${_pcre_existing_target})
        get_target_property(_pcre_existing_type
          ${_pcre_existing_target} TYPE)
        if(_pcre_existing_type STREQUAL "STATIC_LIBRARY")
          set(PCRE_LINKAGE STATIC)
          break()
        elseif(_pcre_existing_type STREQUAL "SHARED_LIBRARY")
          set(PCRE_LINKAGE SHARED)
          break()
        endif()
      endif()
    endforeach()
    unset(_pcre_existing_target)
    unset(_pcre_existing_type)
  endif()
  set(PCRE_FOUND TRUE)
  return()
endif()


include("${CMAKE_CURRENT_LIST_DIR}/GKlibCheckLink.cmake")


#-------------------------------------------------------------------------------
# DISCOVERY AND LINK VALIDATION
#-------------------------------------------------------------------------------
find_path(PCRE_INCLUDE_DIR NAMES pcreposix.h)
find_library(PCRE_POSIX_LIBRARY NAMES pcreposix)
find_library(PCRE_LIBRARY NAMES pcre)

set(_pcre_usable FALSE)
set(_pcre_static FALSE)
set(_pcre_check_language "")
set(_pcre_failure_reason "PCRE headers and libraries were not found")

# A usable package must compile and link from one of the consumer's enabled
# languages. Fortran verifies C symbols with BIND(C), using the linkage recorded
# by the producer because it does not preprocess pcreposix.h.
if(PCRE_INCLUDE_DIR AND PCRE_POSIX_LIBRARY AND PCRE_LIBRARY)
  get_property(_pcre_enabled_languages GLOBAL PROPERTY ENABLED_LANGUAGES)

  if("C" IN_LIST _pcre_enabled_languages)
    set(_pcre_check_language C)
  elseif("CXX" IN_LIST _pcre_enabled_languages)
    set(_pcre_check_language CXX)
  elseif("Fortran" IN_LIST _pcre_enabled_languages AND
      PCRE_LINKAGE MATCHES "^(STATIC|SHARED)$")
    set(_pcre_check_language Fortran)
  else()
    set(_pcre_failure_reason
      "PCRE validation needs C/CXX, or Fortran with PCRE_LINKAGE=STATIC or SHARED")
  endif()

  if(_pcre_check_language)
    set(_pcre_check_source "#include <pcreposix.h>
int main(void) {
  regex_t expression;
  int result = regcomp(&expression, \"x\", REG_EXTENDED);
  if (result == 0) {
    result = regexec(&expression, \"x\", 0, 0, 0);
    regfree(&expression);
  }
  return result;
}")

    if(_pcre_check_language STREQUAL "Fortran")
      # Never execute this probe: null pointers make the calls useful for
      # symbol resolution without inventing a Fortran representation of regex_t.
      set(_pcre_check_source [=[
program check_pcre
  use, intrinsic :: iso_c_binding
  implicit none
  interface
    function compile_regex(r, s, flags) bind(C, name="regcomp") result(status)
      import :: c_ptr, c_int
      type(c_ptr), value :: r, s
      integer(c_int), value :: flags
      integer(c_int) :: status
    end function
    function execute_regex(r, s, n, matches, flags) bind(C, name="regexec") result(status)
      import :: c_ptr, c_int, c_size_t
      type(c_ptr), value :: r, s, matches
      integer(c_size_t), value :: n
      integer(c_int), value :: flags
      integer(c_int) :: status
    end function
    subroutine free_regex(r) bind(C, name="regfree")
      import :: c_ptr
      type(c_ptr), value :: r
    end subroutine
  end interface
  integer(c_int) :: status
  status = compile_regex(c_null_ptr, c_null_ptr, 0_c_int)
  status = status + execute_regex(c_null_ptr, c_null_ptr, 0_c_size_t, c_null_ptr, 0_c_int)
  call free_regex(c_null_ptr)
  if (status == 999) stop 1
end program
]=])
    endif()

    function(_pcre_check_link static_define result)
      cmake_push_check_state(RESET)
      set(CMAKE_REQUIRED_INCLUDES "${PCRE_INCLUDE_DIR}")
      set(CMAKE_REQUIRED_LIBRARIES
        "${PCRE_POSIX_LIBRARY};${PCRE_LIBRARY}")
      if(static_define AND NOT _pcre_check_language STREQUAL "Fortran")
        set(CMAKE_REQUIRED_DEFINITIONS -DPCRE_STATIC)
      endif()

      string(MD5 _pcre_check_id
        "${PCRE_INCLUDE_DIR};${PCRE_POSIX_LIBRARY};${PCRE_LIBRARY};${static_define};${_pcre_check_language}")
      set(_pcre_check_variable "PCRE_LINKS_${_pcre_check_id}")
      gklib_check_link("${_pcre_check_source}" ${_pcre_check_variable}
        LANGUAGE ${_pcre_check_language})
      set(${result} "${${_pcre_check_variable}}" PARENT_SCOPE)
      cmake_pop_check_state()
    endfunction()

    if(_pcre_check_language STREQUAL "Fortran")
      _pcre_check_link(FALSE _pcre_usable)
      if(PCRE_LINKAGE STREQUAL "STATIC")
        set(_pcre_static TRUE)
      endif()
    else()
      _pcre_check_link(FALSE _pcre_links_without_static)
    endif()
    if(_pcre_check_language STREQUAL "Fortran")
      if(NOT _pcre_usable)
        set(_pcre_failure_reason "PCRE C symbols failed the Fortran link check")
      endif()
    elseif(_pcre_links_without_static)
      set(_pcre_usable TRUE)
    else()
      _pcre_check_link(TRUE _pcre_links_with_static)
      if(_pcre_links_with_static)
        set(_pcre_usable TRUE)
        set(_pcre_static TRUE)
      else()
        set(_pcre_failure_reason
          "PCRE libraries failed a compile and link check with and without PCRE_STATIC")
      endif()
    endif()
  endif()
endif()

#-------------------------------------------------------------------------------
# WINDOWS RUNTIME DISCOVERY
#-------------------------------------------------------------------------------
# Shared import libraries are incomplete without their corresponding DLLs.
if(WIN32 AND _pcre_usable AND NOT _pcre_static)
  get_filename_component(_pcre_library_dir "${PCRE_LIBRARY}" DIRECTORY)
  get_filename_component(_pcre_posix_library_dir
    "${PCRE_POSIX_LIBRARY}" DIRECTORY)
  find_file(_pcre_dll
    NAMES pcre.dll libpcre.dll libpcre-1.dll
    HINTS "${_pcre_library_dir}" "${_pcre_library_dir}/../bin"
    NO_DEFAULT_PATH
    NO_CACHE)
  find_file(_pcre_posix_dll
    NAMES pcreposix.dll libpcreposix.dll libpcreposix-0.dll
    HINTS "${_pcre_posix_library_dir}" "${_pcre_posix_library_dir}/../bin"
    NO_DEFAULT_PATH
    NO_CACHE)
  set(PCRE_DLL "${_pcre_dll}")
  set(PCRE_POSIX_DLL "${_pcre_posix_dll}")
  if(NOT _pcre_dll OR NOT _pcre_posix_dll)
    set(_pcre_usable FALSE)
    set(_pcre_failure_reason
      "PCRE import libraries were found, but their runtime DLLs were not")
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCRE
  REQUIRED_VARS
    PCRE_INCLUDE_DIR
    PCRE_POSIX_LIBRARY
    PCRE_LIBRARY
    _pcre_usable
  REASON_FAILURE_MESSAGE "${_pcre_failure_reason}")

#-------------------------------------------------------------------------------
# IMPORTED TARGETS
#-------------------------------------------------------------------------------
# GLOBAL visibility lets GKlib's exported interface reference these dependency
# targets from sibling directories in an embedding project.
if(PCRE_FOUND)
  if(WIN32 AND NOT _pcre_static)
    add_library(PCRE::pcre SHARED IMPORTED GLOBAL)
    set_target_properties(PCRE::pcre PROPERTIES
      IMPORTED_IMPLIB "${PCRE_LIBRARY}"
      IMPORTED_LOCATION "${PCRE_DLL}"
      INTERFACE_INCLUDE_DIRECTORIES "${PCRE_INCLUDE_DIR}")

    add_library(PCRE::pcreposix SHARED IMPORTED GLOBAL)
    set_target_properties(PCRE::pcreposix PROPERTIES
      IMPORTED_IMPLIB "${PCRE_POSIX_LIBRARY}"
      IMPORTED_LOCATION "${PCRE_POSIX_DLL}"
      INTERFACE_INCLUDE_DIRECTORIES "${PCRE_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES PCRE::pcre)
  else()
    function(_pcre_get_library_type library result)
      set(_type UNKNOWN)
      if(_pcre_static OR
        library MATCHES "\\${CMAKE_STATIC_LIBRARY_SUFFIX}$")
        set(_type STATIC)
      elseif(library MATCHES
        "\\${CMAKE_SHARED_LIBRARY_SUFFIX}([.][0-9]+)*$")
        set(_type SHARED)
      endif()
      set(${result} "${_type}" PARENT_SCOPE)
    endfunction()

    _pcre_get_library_type("${PCRE_LIBRARY}" _pcre_library_type)
    _pcre_get_library_type("${PCRE_POSIX_LIBRARY}" _pcre_posix_library_type)

    add_library(PCRE::pcre ${_pcre_library_type} IMPORTED GLOBAL)
    set_target_properties(PCRE::pcre PROPERTIES
      IMPORTED_LOCATION "${PCRE_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${PCRE_INCLUDE_DIR}")

    add_library(PCRE::pcreposix ${_pcre_posix_library_type} IMPORTED GLOBAL)
    set_target_properties(PCRE::pcreposix PROPERTIES
      IMPORTED_LOCATION "${PCRE_POSIX_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${PCRE_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES PCRE::pcre)

    if(WIN32 AND _pcre_static)
      set_property(TARGET PCRE::pcre APPEND PROPERTY
        INTERFACE_COMPILE_DEFINITIONS PCRE_STATIC)
      set_property(TARGET PCRE::pcreposix APPEND PROPERTY
        INTERFACE_COMPILE_DEFINITIONS PCRE_STATIC)
    endif()
  endif()

  add_library(PCRE::POSIX INTERFACE IMPORTED GLOBAL)
  set_target_properties(PCRE::POSIX PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PCRE_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES PCRE::pcreposix)

  set(PCRE_INCLUDE_DIRS "${PCRE_INCLUDE_DIR}")
  set(PCRE_LIBRARIES "${PCRE_POSIX_LIBRARY};${PCRE_LIBRARY}")
  set(PCRE_LINKAGE SHARED)

  if(_pcre_static OR _pcre_library_type STREQUAL "STATIC")
    set(PCRE_LINKAGE STATIC)
  endif()
endif()

# Keep low-level discovery variables available without cluttering normal GUIs.
mark_as_advanced(
  PCRE_INCLUDE_DIR
  PCRE_POSIX_LIBRARY
  PCRE_LIBRARY)
