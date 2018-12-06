set(TARGET test_client)

project(${TARGET})

include_directories(
    "../include"
)

add_executable( 
    ${TARGET}
        "main.cpp"
        "library.h"
)

if(UNIX)
    set_target_properties(
        ${TARGET}
            PROPERTIES
            COMPILE_FLAGS "-std=c++11"
    )
endif()