include(camera_vendors.cmake)

find_package(Qt5 COMPONENTS WebSockets)

set(product.title "${mediaserver.name}")
set(product.display.title "${mediaserver.display.name}")

nx_configure_file(
    maven/filter-resources/mediaserver_core_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resources/static")
nx_copy(IF_NEWER "${NX_SDK_PACKAGE}"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/resources/static")
nx_copy(IF_NEWER "${NX_STORAGE_SDK_PACKAGE}"
    DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/resources/static")

nx_configure_file(
    maven/filter-resources/resources/serverProperties.json
    ${CMAKE_CURRENT_BINARY_DIR}/resources)

nx_configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/webadmin_skins/${defaultSkin}
    ${CMAKE_CURRENT_BINARY_DIR}/resources/static/skin
    COPYONLY)




set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/resources/static/api.xml"
    "${CMAKE_CURRENT_BINARY_DIR}/resources/static/api.json"
    PROPERTIES
        RESOURCE_BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources"
)

set(private_libs qtservice qtsinglecoreapplication sigar)
if(NOT targetDevice STREQUAL edge1)
    list(APPEND private_libs nx_speech_synthesizer)
endif()

nx_add_target(mediaserver_core LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/mediaserver_core_app_info_impl.cpp"
    SOURCE_EXCLUSIONS
        ${vendor_source_exclusions}
    ADDITIONAL_RESOURCES
        "${customization_dir}/mediaserver_core/resources"
        "${customization_dir}/icons/all/favicon.ico"
        "${CMAKE_CURRENT_BINARY_DIR}/resources"
        "${CMAKE_CURRENT_BINARY_DIR}/resources/static/api.xml"
        "${CMAKE_CURRENT_BINARY_DIR}/resources/static/api.json"
    PUBLIC_LIBS
        Qt5::WebSockets
        appserver2 
	nx_onvif 
	nx_email 
	cloud_db_client 
	nx_update
	nx_system_commands
    PRIVATE_LIBS ${private_libs}
)

nx_add_vendor_macros(mediaserver_core)

if(LINUX)
    target_link_libraries(mediaserver_core PRIVATE ldap lber)

    if(arch STREQUAL "aarch64" OR box STREQUAL "edge1")
        target_link_libraries(mediaserver_core PRIVATE sasl2)
    endif()
endif()

if(WINDOWS)
    target_link_libraries(mediaserver_core PRIVATE Wldap32.dll Iphlpapi.dll Crypt32.lib)
endif()
