find_sources(src CPP_FILES)

add_library(client_core
    ${CPP_FILES}
    src/StdAfx.cpp src/StdAfx.h
)

add_precompiled_header(client_core src/StdAfx.h FORCEINCLUDE)

target_include_directories(client_core
    PRIVATE
        ${Qt5Quick_PRIVATE_INCLUDE_DIRS}
    PUBLIC src
)

target_link_libraries(client_core
    PUBLIC
        Qt5::Quick
        appserver2 nx_vms_utils cloud_db_client)
