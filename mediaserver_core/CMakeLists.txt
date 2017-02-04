find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

nx_configure_file(
    maven/filter-resources/mediaserver_core_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resources/static")
rdep_add_package(any/nx_sdk-1.6.0)
rdep_add_package(any/nx_storage_sdk-1.6.0)
nx_copy_if_different(${NX_SDK_PACKAGE} "${CMAKE_CURRENT_BINARY_DIR}/resources/static")
nx_copy_if_different(${NX_STORAGE_SDK_PACKAGE} "${CMAKE_CURRENT_BINARY_DIR}/resources/static")

execute_process(
    COMMAND
        java -jar "$ENV{environment}/bin/apidoctool.jar"
        code-to-xml
        -vms-path "${PROJECT_SOURCE_DIR}"
        -template-xml "${CMAKE_CURRENT_SOURCE_DIR}/api/api_template.xml"
        -output-xml "${CMAKE_CURRENT_BINARY_DIR}/api.xml")
nx_copy_if_different(${CMAKE_CURRENT_BINARY_DIR}/api.xml
    ${CMAKE_CURRENT_BINARY_DIR}/resources/static)

generate_qrc("${CMAKE_CURRENT_BINARY_DIR}/mediaserver_core.qrc"
    "${CMAKE_CURRENT_SOURCE_DIR}/static-resources"
    "${customization_dir}/mediaserver_core/resources"
    "${CMAKE_CURRENT_BINARY_DIR}/resources")

add_library(mediaserver_core
    ${CPP_FILES} ${MOC_FILES}
    src/StdAfx.h src/StdAfx.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/mediaserver_core_app_info_impl.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/mediaserver_core.qrc")

add_precompiled_header(mediaserver_core src/StdAfx.h FORCEINCLUDE)

target_include_directories(mediaserver_core PUBLIC src)

rdep_add_package(onvif)
rdep_add_package(sigar)

target_link_libraries(mediaserver_core
    PUBLIC
        qtservice qtsinglecoreapplication
        onvif sigar
        appserver2 nx_email nx_speech_synthesizer cloud_db_client)

if(LINUX)
    target_link_libraries(mediaserver_core PRIVATE ldap)
endif()
