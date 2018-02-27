cmake_minimum_required(VERSION 3.1)

set(TARGET axiscamplugin)

project(${TARGET})

#set(CMAKE_AUTOMOC ON)
#set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(
    "../../../include"
)

if(WINDOWS)
    add_definitions(
        -D_CRT_SECURE_NO_WARNINGS
    )
endif()

add_library( 
    ${TARGET} SHARED 
        "camera_manager.cpp"
		"camera_manager.h"
		"discovery_manager.cpp"
		"discovery_manager.h"
		"media_encoder.cpp"
		"media_encoder.h"
		"ref_counter.h"
		"rpi_camera.cpp"
		"rpi_camera.h"
		"rpi_omx.cpp"
		"rpi_omx.h"
		"rpi_omx_broadcom.h"
		"rpi_omx_print.cpp"
		"rw_lock.h"
		"stream_reader.cpp"
		"stream_reader.h"
		"timer.h"
		"video_packet.cpp"
		"video_packet.h"
)

#find_package(Qt5Network REQUIRED)
#target_link_libraries(${TARGET} PUBLIC Qt5::Network)

if(WINDOWS)
    target_link_libraries(
        ${TARGET}
        ws2_32
    )
endif()

if(UNIX)
    set_target_properties(
        ${TARGET}
            PROPERTIES
            COMPILE_FLAGS "-std=c++11"
    )
endif()
