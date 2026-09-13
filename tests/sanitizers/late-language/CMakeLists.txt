cmake_minimum_required(VERSION 3.24)
project(GKlibLateLanguageConsumer LANGUAGES C)

# Embed the C library before the parent enables C++. The ASan runtime target
# must delay language-specific final-link options until this outer project ends.
set(GKLIB_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GKLIB_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(GKLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(GKLIB_INSTALL OFF CACHE BOOL "" FORCE)
set(GKLIB_IPO OFF CACHE STRING "" FORCE)
set(GKLIB_SANITIZERS address CACHE STRING "" FORCE)
set(GKLIB_REGEX_BACKEND GKREGEX CACHE STRING "" FORCE)
add_subdirectory("${GKLIB_SOURCE_DIR}" gklib)

enable_language(CXX)
enable_testing()

# Keep a fixture-owned copy in the binary tree. The runner can touch this file
# to force only the consumer to relink when the same CTest case runs again.
configure_file(main.cpp "${CMAKE_CURRENT_BINARY_DIR}/main.cpp" COPYONLY)
add_executable(late-cxx "${CMAKE_CURRENT_BINARY_DIR}/main.cpp")
target_compile_features(late-cxx PRIVATE cxx_std_11)
target_compile_options(late-cxx PRIVATE /fsanitize=address)
target_link_libraries(late-cxx PRIVATE GKlib::GKlib)

add_custom_command(TARGET late-cxx POST_BUILD
  COMMAND "${CMAKE_COMMAND}"
    "-DLIBRARIES=$<TARGET_RUNTIME_DLLS:late-cxx>"
    "-DDESTINATION=$<TARGET_FILE_DIR:late-cxx>"
    -P "${GKLIB_SOURCE_DIR}/cmake/CopyRuntime.cmake"
  VERBATIM)
add_test(NAME late-cxx COMMAND late-cxx)
set_tests_properties(late-cxx PROPERTIES TIMEOUT 30)
