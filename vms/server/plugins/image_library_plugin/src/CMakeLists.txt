cmake_minimum_required(VERSION 3.1)

set(IMAGE_LIB_TARGET image_library_plugin)

project(${IMAGE_LIB_TARGET})

include_directories("../../../include")

if(WINDOWS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_library(${IMAGE_LIB_TARGET} SHARED
        "archive_reader.cpp"
		"archive_reader.h"
		"camera_manager.cpp"
		"camera_manager.h"
		"dir_contents_manager.cpp"
		"dir_contents_manager.h"
		"dir_iterator.cpp"
		"dir_iterator.h"
		"discovery_manager.cpp"
		"discovery_manager.h"
		"ilp_empty_packet.cpp"
		"ilp_empty_packet.h"
		"ilp_video_packet.cpp"
		"ilp_video_packet.h"
		"media_encoder.cpp"
		"media_encoder.h"
		"motion_data_picture.cpp"
		"motion_data_picture.h"
		"mutex.cpp"
		"mutex.h"
		"plugin.cpp"
		"plugin.h"
		"stream_reader.cpp"
		"stream_reader.h"
		"time_periods.cpp"
		"time_periods.h"
		"wildcard_match.cpp"
		"wildcard_match.h"
)

if(WINDOWS)
    target_link_libraries(${IMAGE_LIB_TARGET} ws2_32)
endif()

set_target_properties(${IMAGE_LIB_TARGET} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
