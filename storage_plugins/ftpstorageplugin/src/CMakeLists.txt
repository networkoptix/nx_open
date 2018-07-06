cmake_minimum_required(VERSION 3.1)

set(STORAGE_TARGET ftpstorageplugin)

project(${STORAGE_TARGET})

include_directories("../../../include")

add_definitions(
    -DNOSSL
    -DNOLFS
)

if(WINDOWS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

add_library(${STORAGE_TARGET} SHARED
        "impl/ftplib.h"
        "impl/ftplib.cpp"
        "ftp_library.h"
        "ftp_library.cpp" 
)

if(WINDOWS)
    target_link_libraries(${STORAGE_TARGET} ws2_32)
endif()

set_target_properties(${STORAGE_TARGET} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
