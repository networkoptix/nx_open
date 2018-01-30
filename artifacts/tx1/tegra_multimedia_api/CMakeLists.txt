cmake_minimum_required(VERSION 3.9.0)
project(tegra_video)

# ATTENTION: This cmake file is not intended for building the real libtegra_video.so, which is
# to be built via make on a physical Tegra device - it allows only to build a stub version of
# libtegra_video.so and run those unit tests (on a PC) which do not depend on NVidia libraries.
# Sources of the real libtegra_video.so implementation are registered here for IDE convenience.

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTEGRA_VIDEO_STUB_ONLY=")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if(NOT TARGET nx_kit)
    set(nxKitLibraryType "SHARED" CACHE STRING "" FORCE)
    set(NX_KIT_DIR "../../../open/artifacts/nx_kit")
    include(${NX_KIT_DIR}/nx_kit.cmake)
endif()

include_directories(
    include
    samples/common/algorithm/gie
    samples/common/algorithm/cuda
)

file(GLOB_RECURSE SOURCE_FILES
    LIST_DIRECTORIES true
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    samples/*.h samples/*.cpp include/*.h
)

# The real libtegravideo.so is marked EXCLUDE_FROM_ALL because it is built on a Tegra device.
add_executable(tegra_video_EXCLUDED EXCLUDE_FROM_ALL ${SOURCE_FILES})

add_library(tegra_video SHARED
    samples/04_video_dec_gie/tegra_video_ini.h
    samples/04_video_dec_gie/tegra_video.h
    samples/04_video_dec_gie/tegra_video.cpp
    samples/04_video_dec_gie/tegra_video_stub.h
    samples/04_video_dec_gie/tegra_video_stub.cpp
    samples/04_video_dec_gie/rects_serialization.h
    samples/04_video_dec_gie/rects_serialization.cpp
)
target_include_directories(tegra_video
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/samples/04_video_dec_gie
)
target_link_libraries(tegra_video nx_kit)
if(WIN32)
    set(TEGRA_VIDEO_API_IMPORT_MACRO "__declspec(dllimport)")
    set(TEGRA_VIDEO_API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set_target_properties(tegra_video PROPERTIES CXX_VISIBILITY_PRESET hidden)
    set(TEGRA_VIDEO_API_IMPORT_MACRO "")
    set(TEGRA_VIDEO_API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()
target_compile_definitions(tegra_video
    PRIVATE TEGRA_VIDEO_API=${TEGRA_VIDEO_API_EXPORT_MACRO}
    INTERFACE TEGRA_VIDEO_API=${TEGRA_VIDEO_API_IMPORT_MACRO}
)

if("${withTests}" STREQUAL "")
    set(withTests ON)
endif()

if(withTests)
    add_executable(tegra_video_ut
        samples/04_video_dec_gie/unit_tests/main.cpp
        samples/04_video_dec_gie/net_dimensions.h
        samples/04_video_dec_gie/net_dimensions.cpp
        samples/04_video_dec_gie/unit_tests/net_dimensions_ut.cpp
        samples/04_video_dec_gie/rects_serialization.h
        samples/04_video_dec_gie/rects_serialization.cpp
        samples/04_video_dec_gie/unit_tests/rects_serialization_ut.cpp
        samples/04_video_dec_gie/tegra_video_stub.h
		samples/04_video_dec_gie/tegra_video_stub.cpp
		samples/04_video_dec_gie/unit_tests/tegra_video_stub_ut.cpp
    )
    target_link_libraries(tegra_video_ut nx_kit tegra_video)

    add_test(NAME tegra_video_ut COMMAND tegra_video_ut)
endif()
