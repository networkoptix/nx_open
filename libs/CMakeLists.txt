add_subdirectory(nx_utils)
add_subdirectory(nx_sql)
add_subdirectory(nx_fusion)
add_subdirectory(nx_network)
add_subdirectory(udt)

#if(((LINUX AND arch STREQUAL "x64") OR (WINDOWS AND arch STREQUAL "x64")) AND withClouds)
if(arch STREQUAL "x64" AND withClouds)
    add_subdirectory(nx_cassandra)
endif()

if(withMediaServer)
    add_subdirectory(nx_onvif)
endif()

if(WINDOWS)
    add_subdirectory(nx_gdi_tracer)
endif()
