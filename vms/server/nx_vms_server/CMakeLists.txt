include(camera_vendors.cmake)
include(find_java)

find_package(Qt5 COMPONENTS WebSockets)

nx_configure_file(
    maven/filter-resources/mediaserver_core_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_configure_file(
    maven/filter-resources/resources/serverProperties.json
    ${CMAKE_CURRENT_BINARY_DIR}/serverProperties.json)

set(apidoctool_jar "${APIDOCTOOL_PATH}/apidoctool.jar")

set(apidoctool_params
    "-config" "${CMAKE_CURRENT_SOURCE_DIR}/api/apidoctool.properties"
)

if(Java_VERSION_MINOR LESS 6 OR Java_VERSION_MINOR GREATER 8)
    message(FATAL_ERROR "Java Runtime Environment 6, 7 or 8 is required for apidoctool.")
endif()

execute_process(
    COMMAND "${Java_JAVA_EXECUTABLE}" -jar "${apidoctool_jar}" print-deps ${apidoctool_params}
    RESULT_VARIABLE apidoctool_result
    OUTPUT_VARIABLE apidoctool_deps
)

if(NOT apidoctool_result EQUAL 0)
    message(FATAL_ERROR "Cannot get dependencies list from apidoctool.")
endif()

nx_split_string(apidoctool_deps "${apidoctool_deps}")
set(apidoctool_deps_absolute)
foreach(dependency ${apidoctool_deps})
    list(APPEND apidoctool_deps_absolute "${PROJECT_SOURCE_DIR}/${dependency}")
endforeach()

add_custom_command(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/api.xml
        ${CMAKE_CURRENT_BINARY_DIR}/api.json
    DEPENDS
        ${apidoctool_jar}
        ${CMAKE_CURRENT_SOURCE_DIR}/api/api_template.xml
        ${apidoctool_deps_absolute}
    COMMAND
        ${Java_JAVA_EXECUTABLE} -jar ${apidoctool_jar}
        code-to-xml
        -vms-path ${PROJECT_SOURCE_DIR}
        -template-xml ${CMAKE_CURRENT_SOURCE_DIR}/api/api_template.xml
        -output-xml ${CMAKE_CURRENT_BINARY_DIR}/api.xml
        -output-json ${CMAKE_CURRENT_BINARY_DIR}/api.json
        ${apidoctool_params}
)

set(static_resources
    ${CMAKE_CURRENT_BINARY_DIR}/api.xml
    ${CMAKE_CURRENT_BINARY_DIR}/api.json
    ${customization_dir}/license.html
    ${NX_SDK_PACKAGE}
    ${NX_STORAGE_SDK_PACKAGE}
    ${CMAKE_SOURCE_DIR}/specific_features.txt
)
set_source_files_properties(${static_resources} PROPERTIES RESOURCE_ALIAS_PREFIX static/)

set(favicon ${customization_dir}/icons/all/favicon.ico)
set_property(SOURCE ${favicon} PROPERTY RESOURCE_ALIAS_PREFIX static/customization/)

set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/webadmin_skins/${defaultSkin}
    PROPERTY RESOURCE_ALIAS_PREFIX static/skin/)

nx_add_target(nx_vms_server LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/mediaserver_core_app_info_impl.cpp"
    SOURCE_EXCLUSIONS
        ${vendor_source_exclusions}
    ADDITIONAL_RESOURCES
        ${customization_dir}/mediaserver_core/resources
        ${CMAKE_CURRENT_SOURCE_DIR}/webadmin_skins/${defaultSkin}
        ${favicon}
        ${static_resources}
        ${CMAKE_CURRENT_BINARY_DIR}/serverProperties.json
    PUBLIC_LIBS
        nx_kit #< This dependency should come first to satisfy OutputRedirector.
        Qt5::WebSockets
        appserver2
        nx_vms_server_db
        nx_onvif
        nx_email
        cloud_db_client
        nx_update
        nx_system_commands
        nx_plugin_utils
    PRIVATE_LIBS
        Qt5::Qml
        qtservice
        qtsinglecoreapplication
        sigar
        nx_speech_synthesizer
)

nx_force_include(nx_vms_server
    ${CMAKE_SOURCE_DIR}/libs/nx_utils/src/nx/utils/literal.h
    ${CMAKE_SOURCE_DIR}/libs/nx_utils/src/nx/utils/log/log_message.h
)

nx_add_vendor_macros(nx_vms_server)

if(LINUX)
    target_link_libraries(nx_vms_server PRIVATE ldap lber)
endif()

if(WINDOWS)
    target_link_libraries(nx_vms_server PRIVATE Wldap32.dll Iphlpapi.dll Crypt32.lib)
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()
