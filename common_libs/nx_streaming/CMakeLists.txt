find_sources(src CPP_FILES)

add_library(nx_streaming
    ${CPP_FILES}
    src/StdAfx.cpp src/StdAfx.h
)

add_precompiled_header(nx_streaming src/StdAfx.h FORCEINCLUDE)

target_include_directories(nx_streaming
    PRIVATE ${CMAKE_SOURCE_DIR}/common_libs/nx_network/src
    PUBLIC src)

target_compile_definitions(nx_streaming
    PRIVATE NX_NETWORK_API=${API_IMPORT_MACRO}
)

target_link_libraries(nx_streaming PUBLIC common)
