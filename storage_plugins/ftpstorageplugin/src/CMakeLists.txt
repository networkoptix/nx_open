set(TARGET ftp_library)

project(${TARGET})

include_directories(
    "../include"
)

add_definitions(
    -DSTORAGE_EXPORTS
    -DNOSSL
    -DNOLFS
)

add_library( 
    ${TARGET} SHARED 
        "impl/ftplib.h"
        "impl/ftplib.cpp"
        "ftp_library.h"
        "ftp_library.cpp" 
)

target_link_libraries(
    ${TARGET}
        ws2_32
)

if(UNIX)
    set_target_properties(
        ${TARGET}
            PROPERTIES
            COMPILE_FLAGS "-std=c++11"
    )
endif()