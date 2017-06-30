cmake_minimum_required(VERSION 2.8)

set(TARGET ftpstorageplugin)

project(${TARGET})

include_directories(
    "../../../include"
)

add_definitions(
    -DNOSSL
    -DNOLFS
)

if(WINDOWS)
    add_definitions(
        -D_CRT_SECURE_NO_WARNINGS
    )
endif()

add_library( 
    ${TARGET} SHARED 
        "impl/ftplib.h"
        "impl/ftplib.cpp"
        "ftp_library.h"
        "ftp_library.cpp" 
)

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
