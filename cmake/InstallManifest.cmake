# Capture only this project's install rules after external dependencies.
function(gklib_install_manifest_begin)
  set(manifest_project GKlib)
  set(manifest_directory "${PROJECT_BINARY_DIR}/install-manifests")
  configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RecordInstall.cmake.in"
    "${PROJECT_BINARY_DIR}/RecordInstall.cmake" @ONLY)
  install(CODE "list(LENGTH CMAKE_INSTALL_MANIFEST_FILES _GKlib_install_start)"
    ALL_COMPONENTS)
endfunction()

# Append the recorder after GKlib's install rules so it sees the complete slice.
function(gklib_install_manifest_end)
  install(SCRIPT "${PROJECT_BINARY_DIR}/RecordInstall.cmake" ALL_COMPONENTS)
endfunction()
