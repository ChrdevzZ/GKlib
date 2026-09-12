cmake_minimum_required(VERSION 3.24)
project(GKlibSubprojectConsumer LANGUAGES C)

# Seed parent policies that GKlib must inherit or explicitly override.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
set(GKLIB_IPO OFF CACHE STRING "")

add_subdirectory(../.. gklib)

# Subproject defaults must not create standalone build or delivery targets.
if(GKLIB_BUILD_PROGRAMS OR GKLIB_BUILD_TESTING OR GKLIB_INSTALL)
  message(FATAL_ERROR "GKlib subproject-only features must default to OFF")
endif()
if(TARGET strings OR TARGET gklib-api-test OR
  TARGET gklib-uninstall OR TARGET package_source)
  message(FATAL_ERROR "GKlib added a top-level-only target to its parent")
endif()

# Confirm the target kept the requested IPO policy and inherited parent PIC.
get_target_property(_gklib_ipo GKlib INTERPROCEDURAL_OPTIMIZATION)
get_target_property(_gklib_pic GKlib POSITION_INDEPENDENT_CODE)
if(_gklib_ipo)
  message(FATAL_ERROR "GKLIB_IPO=OFF did not override the inherited IPO setting")
endif()
if(_gklib_pic)
  message(FATAL_ERROR "GKlib did not inherit the parent PIC setting")
endif()

# Link through the public alias exposed to embedding projects.
add_executable(gklib-subproject-consumer ../package/package_consumer.c)
target_link_libraries(gklib-subproject-consumer PRIVATE GKlib::GKlib)
