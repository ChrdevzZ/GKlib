cmake_minimum_required(VERSION 3.24)
project(GKlibTemplateConsumer LANGUAGES C CXX)

find_package(GKlib CONFIG REQUIRED)

# Instantiate both public template variants through the installed target.
add_executable(gklib-template-consumer-c consumer.c)
add_executable(gklib-template-consumer-cxx consumer.cpp)
target_compile_features(gklib-template-consumer-c PRIVATE c_std_99)
target_compile_features(gklib-template-consumer-cxx PRIVATE cxx_std_11)
target_link_libraries(gklib-template-consumer-c PRIVATE GKlib::GKlib)
target_link_libraries(gklib-template-consumer-cxx PRIVATE GKlib::GKlib)

# Treat warnings as fixture failures for MSVC-compatible frontends.
if(MSVC OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC" OR
  CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  target_compile_options(gklib-template-consumer-c PRIVATE /WX)
  target_compile_options(gklib-template-consumer-cxx PRIVATE /WX)
endif()

# The consumer owns execution tests for its two local targets.
enable_testing()
add_test(NAME GKlib.template-consumer-c COMMAND gklib-template-consumer-c)
add_test(NAME GKlib.template-consumer-cxx COMMAND gklib-template-consumer-cxx)
set_tests_properties(GKlib.template-consumer-c GKlib.template-consumer-cxx
  PROPERTIES TIMEOUT 30)

# Add the imported shared-library directory to each Windows test environment.
if(WIN32)
  set_tests_properties(GKlib.template-consumer-c GKlib.template-consumer-cxx
    PROPERTIES ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:GKlib::GKlib>")
endif()
