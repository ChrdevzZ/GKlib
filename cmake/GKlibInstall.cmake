include(CMakePackageConfigHelpers)


#-------------------------------------------------------------------------------
# LICENSES
#-------------------------------------------------------------------------------
install(FILES
  "${PROJECT_SOURCE_DIR}/LICENSE.txt"
  "${PROJECT_SOURCE_DIR}/LICENSES.md"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/GKlib"
  COMPONENT GKlib_Development)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/LICENSES/"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/GKlib"
  COMPONENT GKlib_Development)

#-------------------------------------------------------------------------------
# CMAKE PACKAGE
#-------------------------------------------------------------------------------
# Cache the package directory so distributions can place CMake metadata in a
# platform-specific location without changing the library/include layout.
set(GKLIB_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/GKlib" CACHE STRING
  "Install directory for GKlib CMake package files")
configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/GKlibConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/cmake/GKlibConfig.cmake"
  INSTALL_DESTINATION "${GKLIB_INSTALL_CMAKEDIR}")
write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/cmake/GKlibConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(EXPORT GKlibTargets
  FILE GKlibTargets.cmake
  NAMESPACE GKlib::
  DESTINATION "${GKLIB_INSTALL_CMAKEDIR}"
  COMPONENT GKlib_Development)
install(FILES
  "${PROJECT_BINARY_DIR}/cmake/GKlibConfig.cmake"
  "${PROJECT_BINARY_DIR}/cmake/GKlibConfigVersion.cmake"
  "${PROJECT_SOURCE_DIR}/cmake/FindPCRE.cmake"
  "${PROJECT_SOURCE_DIR}/cmake/GKlibCheckLink.cmake"
  DESTINATION "${GKLIB_INSTALL_CMAKEDIR}"
  COMPONENT GKlib_Development)

#-------------------------------------------------------------------------------
# UNINSTALL TARGET
#-------------------------------------------------------------------------------
# The script consumes GKlib-owned manifest records created by install rules.
set(manifest_project GKlib)
set(manifest_directory "${PROJECT_BINARY_DIR}/install-manifests")
configure_file(
  "${PROJECT_SOURCE_DIR}/cmake/uninstall.cmake.in"
  "${PROJECT_BINARY_DIR}/cmake/uninstall.cmake"
  @ONLY)
add_custom_target(gklib-uninstall
  COMMAND "${CMAKE_COMMAND}" -P "${PROJECT_BINARY_DIR}/cmake/uninstall.cmake"
  COMMENT "Uninstalling files recorded by GKlib")

# Install only the portable discovery module, never producer machine paths.
install(FILES "${PROJECT_SOURCE_DIR}/cmake/FindIntelRuntime.cmake"
  DESTINATION "${GKLIB_INSTALL_CMAKEDIR}" COMPONENT GKlib_Development)

if(GKLIB_CONFIG_NEEDS_INTEL_RUNTIME)
  intel_runtime_install(GKlib GKlib "${GKLIB_INSTALL_CMAKEDIR}" GKlib_Development)
endif()

# Static archives retain their producer's OpenMP runtime, without exporting
# compiler flags or absolute paths from that producer's SDK.
if(GKLIB_CONFIG_NEEDS_OPENMP)
  gklib_openmp_install("${GKLIB_INSTALL_CMAKEDIR}")
endif()
