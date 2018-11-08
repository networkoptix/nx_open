set(library_type)
if(WINDOWS)
    set(library_type LIBRARY_TYPE SHARED)
endif()

nx_configure_file(
    app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(nx_network LIBRARY ${library_type} NO_MOC
    WERROR_IF NOT WINDOWS AND NOT ANDROID AND NOT IOS
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/app_info_impl.cpp"
    PUBLIC_LIBS
        Qt5::Concurrent
        Qt5::Network
        openssl
        nx_kit
        nx_utils nx_fusion udt
)

set(mocableNetworkFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/network/public_ip_discovery.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/network/http/async_http_client_reply.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/network/deprecated/asynchttpclient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/network/upnp/upnp_device_searcher.h)

nx_add_qt_mocables(nx_network ${mocableNetworkFiles}
    INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(nx_network
    PRIVATE NX_NETWORK_API=${API_EXPORT_MACRO}
    INTERFACE NX_NETWORK_API=${API_IMPORT_MACRO})

if(WINDOWS)
    target_link_libraries(nx_network PRIVATE Iphlpapi ws2_32)
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
