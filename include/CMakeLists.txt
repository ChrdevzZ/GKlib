#-------------------------------------------------------------------------------
# PUBLIC HEADERS
#-------------------------------------------------------------------------------
set(GKLIB_PUBLIC_HEADERS
  GKlib.h
  gk_arch.h
  gk_defs.h
  gk_externs.h
  gk_getopt.h
  gk_macros.h
  gk_mkblas.h
  gk_mkmemory.h
  gk_mkpqueue.h
  gk_mkpqueue2.h
  gk_mkrandom.h
  gk_mksort.h
  gk_mkutils.h
  gk_proto.h
  gk_struct.h
  gk_types.h
  gkregex.h
  gk_ms_stat.h)

list(TRANSFORM GKLIB_PUBLIC_HEADERS PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
set(GKLIB_PUBLIC_HEADERS "${GKLIB_PUBLIC_HEADERS}" PARENT_SCOPE)

# Generate configuration and export declarations in the build include tree.
set(GKLIB_GENERATED_INCLUDE_DIR "${PROJECT_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${GKLIB_GENERATED_INCLUDE_DIR}")
configure_file(gklib_config.h.in
  "${GKLIB_GENERATED_INCLUDE_DIR}/gklib_config.h")
set(GKLIB_GENERATED_INCLUDE_DIR "${GKLIB_GENERATED_INCLUDE_DIR}" PARENT_SCOPE)

# Header installation belongs to the development component.
if(GKLIB_INSTALL)
  install(FILES
    ${GKLIB_PUBLIC_HEADERS}
    "${GKLIB_GENERATED_INCLUDE_DIR}/gklib_config.h"
    "${GKLIB_GENERATED_INCLUDE_DIR}/gklib_export.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT GKlib_Development)
endif()
