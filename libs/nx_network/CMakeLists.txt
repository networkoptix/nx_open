## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if(MACOSX OR IOS)
    set(mac_sources
        src/nx/network/ssl/certificate_mac.mm
    )
endif()

nx_add_target(nx_network LIBRARY NO_MOC
    ADDITIONAL_SOURCES
        ${mac_sources}
    PUBLIC_LIBS
        Qt6::Network
        OpenSSL::OpenSSL
        nx_kit
        nx_utils
        nx_reflect
    PRIVATE_LIBS
        udt
    FOLDER common/libs
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
    target_link_libraries(nx_network
        PRIVATE
            Iphlpapi
        PUBLIC
            ws2_32
    )
endif()

if(MACOSX OR IOS)
    target_link_libraries(nx_network
        PRIVATE
            "-framework Security"
    )
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
