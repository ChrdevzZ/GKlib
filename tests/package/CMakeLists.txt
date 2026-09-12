cmake_minimum_required(VERSION 3.24)
project(GKlibPackageConsumer LANGUAGES C CXX)

find_package(GKlib CONFIG REQUIRED)

# Exercise the exported target from both supported consumer languages.
add_executable(gklib-package-c package_consumer.c)
target_link_libraries(gklib-package-c PRIVATE GKlib::GKlib)

add_executable(gklib-package-cxx package_consumer.cpp)
target_compile_features(gklib-package-cxx PRIVATE cxx_std_11)
target_link_libraries(gklib-package-cxx PRIVATE GKlib::GKlib)
