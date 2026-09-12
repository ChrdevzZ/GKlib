# Source packages are owned by standalone GKlib builds at the project root.
set(CPACK_PACKAGE_NAME GKlib)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
# Preserve regex escapes when CPack writes and reloads its configuration.
set(CPACK_VERBATIM_VARIABLES TRUE)
set(CPACK_SOURCE_IGNORE_FILES
  "/[.]git($|/);/build[^/]*/;/out/;/[.]aijournal/;/[.]aisources/;CMakeUserPresets.json;/__pycache__/;[.]pyc$;~$;${CPACK_SOURCE_IGNORE_FILES}")

# CPack has its own exclusions; .gitignore alone does not protect archives.
# Include the actual binary directory when users choose a nonstandard name.
set(_gklib_binary_pattern "${PROJECT_BINARY_DIR}")
foreach(character "." "+" "*" "?" "^" "$" "(" ")" "[" "]" "|")
  string(REPLACE "${character}" "\\${character}"
    _gklib_binary_pattern "${_gklib_binary_pattern}")
endforeach()
list(APPEND CPACK_SOURCE_IGNORE_FILES
  "${_gklib_binary_pattern}/"
  "[.](obj|o|a|lib|dll|exe|pdb|ilk|exp|pyc)$")
include(CPack)
