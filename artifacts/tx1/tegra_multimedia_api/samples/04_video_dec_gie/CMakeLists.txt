cmake_minimum_required(VERSION 3.8.0)
project(net_dimensions_ut)

# This cmake file can be used to build and run the tests on a PC (tested with CLion), because the
# tested code is pure C++ and does not depend on any NVidia libraries.

set(CMAKE_CXX_STANDARD 11)

# Current dir: VMS_DIR/artifacts/tx1/tegra_multimedia_api/samples/04_video_dec_gie/
set(NX_KIT_DIR "../../../../../open/artifacts/nx_kit")

include("${NX_KIT_DIR}/nx_kit.cmake")

include_directories(.)

add_executable(net_dimensions_ut
    net_dimensions_ut.cpp
    net_dimensions.h
    net_dimensions.cpp
)

target_link_libraries(net_dimensions_ut nx_kit)
