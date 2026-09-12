#-------------------------------------------------------------------------------
# COMMAND-LINE PROGRAMS
#-------------------------------------------------------------------------------
set(GKLIB_PROGRAMS
  strings
  gksort
  fis
  gkrw
  gkgraph
  csrcnv
  grKx
  m2mnbrs
  cmpnbrs
  splatt2svd
  gkuniq)

foreach(prog ${GKLIB_PROGRAMS})
  add_executable(${prog} ${prog}.c)
  target_link_libraries(${prog} GKlib::GKlib)
  gklib_target_ipo(${prog})

  # Make build-tree programs runnable beside shared GKlib dependencies.
  if(WIN32)
    add_custom_command(TARGET ${prog} POST_BUILD
      COMMAND ${CMAKE_COMMAND}
        "-DLIBRARIES=$<TARGET_RUNTIME_DLLS:${prog}>"
        "-DDESTINATION=$<TARGET_FILE_DIR:${prog}>"
        -P "${PROJECT_SOURCE_DIR}/cmake/CopyRuntime.cmake"
      VERBATIM)
  endif()
endforeach()

# Installation is optional so subproject builds do not claim application paths.
if(GKLIB_INSTALL)
  install(TARGETS ${GKLIB_PROGRAMS}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      COMPONENT GKlib_Applications)
endif()
