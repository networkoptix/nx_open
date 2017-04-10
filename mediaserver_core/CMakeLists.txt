rdep_add_package(any/nx_sdk-1.6.0)
rdep_add_package(any/nx_storage_sdk-1.6.0)
rdep_add_package(onvif)
rdep_add_package(sigar)

set(product.title "${mediaserver.name}")
set(product.display.title "${mediaserver.display.name}")

nx_configure_file(
    maven/filter-resources/mediaserver_core_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resources/static")
nx_copy_if_different(${NX_SDK_PACKAGE} "${CMAKE_CURRENT_BINARY_DIR}/resources/static")
nx_copy_if_different(${NX_STORAGE_SDK_PACKAGE} "${CMAKE_CURRENT_BINARY_DIR}/resources/static")

execute_process(
    COMMAND
        java -jar "${APIDOCTOOL_PATH}/apidoctool.jar"
        code-to-xml
        -vms-path "${PROJECT_SOURCE_DIR}"
        -template-xml "${CMAKE_CURRENT_SOURCE_DIR}/api/api_template.xml"
        -output-xml "${CMAKE_CURRENT_BINARY_DIR}/api.xml")
nx_copy_if_different(${CMAKE_CURRENT_BINARY_DIR}/api.xml
    ${CMAKE_CURRENT_BINARY_DIR}/resources/static)

nx_add_target(mediaserver_core LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/mediaserver_core_app_info_impl.cpp"
    ADDITIONAL_RESOURCES
        "${customization_dir}/mediaserver_core/resources"
        "${CMAKE_CURRENT_BINARY_DIR}/resources"
    PUBLIC_LIBS
        qtservice qtsinglecoreapplication
        onvif sigar
        appserver2 nx_email nx_speech_synthesizer cloud_db_client)

if(LINUX)
    target_link_libraries(mediaserver_core PRIVATE ldap)
endif()

if(WIN32)
    target_link_libraries(mediaserver_core PRIVATE Wldap32.dll Iphlpapi.dll Crypt32.lib)
endif()
