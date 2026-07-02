cmake_minimum_required(VERSION 3.24)
project(ChinaskiVirtualCamera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ChinaskiVirtualCamera SHARED
  src/virtual_camera_registration.cpp
)

target_include_directories(ChinaskiVirtualCamera PRIVATE include)

add_executable(chinaski-vcamctl
  src/vcamctl.cpp
  src/virtual_camera_registration.cpp
)

target_include_directories(chinaski-vcamctl PRIVATE include)

if(WIN32)
  target_link_libraries(ChinaskiVirtualCamera PRIVATE mfplat mfuuid ole32 runtimeobject)
  target_link_libraries(chinaski-vcamctl PRIVATE mfplat mfuuid ole32 runtimeobject)
  target_compile_definitions(ChinaskiVirtualCamera PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_definitions(chinaski-vcamctl PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
