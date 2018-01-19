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

set(apidoctool_jar "${APIDOCTOOL_PATH}/apidoctool.jar")

execute_process(
    COMMAND java -jar "${apidoctool_jar}" print-deps
    RESULT_VARIABLE apidoctool_result
    OUTPUT_VARIABLE apidoctool_deps
)

if(NOT apidoctool_result EQUAL 0)
    message(FATAL ERROR "Cannot get dependencies list from apidoctool.")
endif()

separate_arguments(apidoctool_deps)
set(apidoctool_deps_absolute)
foreach(dependency ${apidoctool_deps})
    list(APPEND apidoctool_deps_absolute "${PROJECT_SOURCE_DIR}/${dependency}")
endforeach()

add_custom_command(
    OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/resources/static/api.xml"
        "${CMAKE_CURRENT_BINARY_DIR}/resources/static/api.json"
    MAIN_DEPENDENCY "${apidoctool_jar}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/api/api_template.xml"
        ${apidoctool_deps_absolute}
    COMMAND
        java -jar "${apidoctool_jar}"
        code-to-xml
        -vms-path "${PROJECT_SOURCE_DIR}"
        -template-xml "${CMAKE_CURRENT_SOURCE_DIR}/api/api_template.xml"
        -output-xml "${CMAKE_CURRENT_BINARY_DIR}/api.xml"
        -output-json "${CMAKE_CURRENT_BINARY_DIR}/api.json"
    COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/api.xml"
            "${CMAKE_CURRENT_BINARY_DIR}/resources/static/api.xml"
    COMMAND
        ${CMAKE_COMMAND} -E remove "${CMAKE_CURRENT_BINARY_DIR}/api.xml"
    COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/api.json"
            "${CMAKE_CURRENT_BINARY_DIR}/resources/static/api.json"
    COMMAND
        ${CMAKE_COMMAND} -E remove "${CMAKE_CURRENT_BINARY_DIR}/api.json"
)

set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/resources/static/api.xml"
    "${CMAKE_CURRENT_BINARY_DIR}/resources/static/api.json"
    PROPERTIES
        RESOURCE_BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/resources"
)

nx_add_target(mediaserver_core LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/mediaserver_core_app_info_impl.cpp"
    ADDITIONAL_RESOURCES
        "${customization_dir}/mediaserver_core/resources"
        "${customization_dir}/icons/all/favicon.ico"
        "${CMAKE_CURRENT_BINARY_DIR}/resources"
        "${CMAKE_CURRENT_BINARY_DIR}/resources/static/api.xml"
        "${CMAKE_CURRENT_BINARY_DIR}/resources/static/api.json"
    PUBLIC_LIBS
        qtservice qtsinglecoreapplication Qt5::WebSockets
        onvif sigar
        appserver2 nx_email nx_speech_synthesizer cloud_db_client)

if(LINUX)
    target_link_libraries(mediaserver_core PRIVATE ldap)

    if(arch MATCHES "arm|aarch64")
        target_link_libraries(mediaserver_core PRIVATE sasl2)
    endif()
endif()

if(WINDOWS)
    target_link_libraries(mediaserver_core PRIVATE Wldap32.dll Iphlpapi.dll Crypt32.lib)
endif()
