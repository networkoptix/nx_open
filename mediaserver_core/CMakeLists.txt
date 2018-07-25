include(camera_vendors.cmake)
include(find_java)

find_package(Qt5 COMPONENTS WebSockets)

nx_configure_file(
    maven/filter-resources/mediaserver_core_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

nx_configure_file(
    maven/filter-resources/resources/serverProperties.json
    ${CMAKE_CURRENT_BINARY_DIR}/serverProperties.json)

nx_configure_file(
   ${customization_dir}/icons/all/favicon.ico
   ${CMAKE_CURRENT_BINARY_DIR}/resources/static/customization
   COPYONLY)

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
    MAIN_DEPENDENCY ${apidoctool_jar}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/api/api_template.xml
        ${apidoctool_deps_absolute}
    COMMAND
        ${Java_JAVA_EXECUTABLE} -jar ${apidoctool_jar}
        code-to-xml
        -vms-path ${PROJECT_SOURCE_DIR}
        -template-xml ${CMAKE_CURRENT_SOURCE_DIR}/api/api_template.xml
        -output-xml ${CMAKE_CURRENT_BINARY_DIR}/api.xml.copy
        -output-json ${CMAKE_CURRENT_BINARY_DIR}/api.json.copy
        ${apidoctool_params}
    COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_BINARY_DIR}/api.xml.copy
            ${CMAKE_CURRENT_BINARY_DIR}/api.xml
    COMMAND
        ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/api.xml.copy
    COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_BINARY_DIR}/api.json.copy
            ${CMAKE_CURRENT_BINARY_DIR}/api.json
    COMMAND
        ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/api.json.copy
)

set(private_libs qtservice qtsinglecoreapplication sigar)
if(NOT targetDevice STREQUAL edge1)
    list(APPEND private_libs nx_speech_synthesizer)
endif()

set(static_resources
    ${CMAKE_CURRENT_BINARY_DIR}/api.xml
    ${CMAKE_CURRENT_BINARY_DIR}/api.json
    ${customization_dir}/license.html
    ${NX_SDK_PACKAGE}
    ${NX_STORAGE_SDK_PACKAGE}
)
set_source_files_properties(${static_resources} PROPERTIES RESOURCE_ALIAS_PREFIX static/)

set(favicon ${customization_dir}/icons/all/favicon.ico)
set_property(SOURCE ${favicon} PROPERTY RESOURCE_ALIAS_PREFIX static/customization/)

set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/webadmin_skins/${defaultSkin}
    PROPERTY RESOURCE_ALIAS_PREFIX static/skin/)

nx_add_target(mediaserver_core LIBRARY
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
        Qt5::WebSockets
        appserver2
        mediaserver_db
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
endif()

if(WINDOWS)
    target_link_libraries(mediaserver_core PRIVATE Wldap32.dll Iphlpapi.dll Crypt32.lib)
endif()
