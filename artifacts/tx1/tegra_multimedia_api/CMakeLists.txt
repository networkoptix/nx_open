cmake_minimum_required(VERSION 3.9.0)
project(tegra_video)

# ATTENTION: This cmake file is not intended for building the real libtegra_video.so, which is
# to be built via make on a physical Tegra device - it allows only to build a stub version of
# libtegra_video.so and run those unit tests (on a PC) which do not depend on NVidia libraries.
# Sources of the real libtegra_video.so implementation are registered here for IDE convenience.

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEGRA_VIDEO_API= -DTEGRA_VIDEO_STUB_ONLY=")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(NX_KIT_DIR "../../../open/artifacts/nx_kit")

include(${NX_KIT_DIR}/nx_kit.cmake)

include_directories(
    ${NX_KIT_DIR}/src
    include
    samples/common/algorithm/gie
    samples/common/algorithm/cuda
    samples/04_video_dec_gie
)

file(GLOB_RECURSE SOURCE_FILES
    LIST_DIRECTORIES true
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    samples/*.h samples/*.cpp include/*.h
)

# The real libtegravideo.so is marked EXCLUDE_FROM_ALL because it is built on a Tegra device.
add_executable(tegra_video EXCLUDE_FROM_ALL ${SOURCE_FILES})

add_library(tegra_video_stub
    samples/04_video_dec_gie/tegra_video_ini.h
    samples/04_video_dec_gie/tegra_video.h
    samples/04_video_dec_gie/tegra_video.cpp
    samples/04_video_dec_gie/tegra_video_stub.cpp
)
target_link_libraries(tegra_video_stub nx_kit)

add_executable(tegra_video_ut
    samples/04_video_dec_gie/unit_tests/main.cpp
    samples/04_video_dec_gie/net_dimensions.h
    samples/04_video_dec_gie/net_dimensions.cpp
    samples/04_video_dec_gie/unit_tests/net_dimensions_ut.cpp
    samples/04_video_dec_gie/unit_tests/tegra_video_stub_ut.cpp
)
target_link_libraries(tegra_video_ut nx_kit tegra_video_stub)
