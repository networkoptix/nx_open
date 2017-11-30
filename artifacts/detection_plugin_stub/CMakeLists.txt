cmake_minimum_required(VERSION 3.8)

project(detection_plugin_stub)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../detection_plugin_interface)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(detection_plugin SHARED detection_plugin_stub.cpp)
