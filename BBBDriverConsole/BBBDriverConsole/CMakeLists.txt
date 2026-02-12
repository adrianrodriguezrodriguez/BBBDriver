cmake_minimum_required(VERSION 3.16)
project(BBBDriverConsole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SPINNAKER_ROOT "/opt/spinnaker")

add_executable(BBBDriverConsole
  main.cpp
  BBBDriver.cpp
  BBBConfig.cpp
  BBBPointCloudFilters.cpp
  BBBVisionMath.cpp
  BBBImageIO.cpp
  pch.cpp
)

target_include_directories(BBBDriverConsole PRIVATE
  ${SPINNAKER_ROOT}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_directories(BBBDriverConsole PRIVATE
  ${SPINNAKER_ROOT}/lib
)

target_link_libraries(BBBDriverConsole PRIVATE
  Spinnaker
  pthread
  dl
)

set_target_properties(BBBDriverConsole PROPERTIES
  BUILD_RPATH "${SPINNAKER_ROOT}/lib"
)
