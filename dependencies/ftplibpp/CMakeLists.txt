## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(ftplibpp_dir ${open_source_root}/artifacts/ftplibpp)

add_library(ftplibpp SHARED
    ${ftplibpp_dir}/ftplibpp/ftplib.h
    ${ftplibpp_dir}/ftplibpp/ftplib.cpp
)
if(WINDOWS)
    target_link_libraries(ftplibpp PRIVATE Ws2_32)
endif()
target_include_directories(ftplibpp PUBLIC ${ftplibpp_dir})
set_target_properties(ftplibpp PROPERTIES
    OUTPUT_NAME ftp++ #< This name is used in the original Makefile of this lib.
    FOLDER third_party
    CXX_VISIBILITY_PRESET default
)

target_compile_definitions(ftplibpp
    PUBLIC
        -DNOSSL
        -DNOLFS
    PRIVATE
        -DBUILDING_DLL
)

if(LINUX)
    target_compile_options(ftplibpp PRIVATE
        -Wno-stringop-truncation
    )
endif()
