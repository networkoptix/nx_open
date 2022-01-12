## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(Qt6 COMPONENTS Multimedia Quick ShaderTools)

set(qt_helpers_root "${open_source_root}/artifacts/qt_helpers/src")
file(GLOB_RECURSE qt_helpers_sources "${qt_helpers_root}/qt_helpers/*")
file(GLOB_RECURSE qt_helpers_headers "${qt_helpers_root}/qt_helpers/*.h")

nx_add_target(nx_vms_client_core LIBRARY
    ADDITIONAL_SOURCES
        ${qt_helpers_sources}
    PUBLIC_LIBS
        Qt6::Multimedia
        Qt6::Quick
        appserver2
        cloud_db_client
        nx_media
        nx_network
        nx_utils
        nx_vms_license
        nx_vms_rules
        nx_vms_utils
        ffmpeg::ffmpeg
        speex
    ADDITIONAL_MOC_INCLUDE_DIRS
        ${Qt6Qml_INCLUDE_DIRS}
    ADDITIONAL_MOCABLES
        ${qt_helpers_headers}
    FOLDER client/libs
)
nx_make_target_translatable(nx_vms_client_core)

target_compile_definitions(nx_vms_client_core
    PRIVATE NX_VMS_CLIENT_CORE_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_CLIENT_CORE_API=${API_IMPORT_MACRO})

file(GLOB_RECURSE shader_sources CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "static-resources/qml/*.vert"
    "static-resources/qml/*.frag"
)
qt6_add_shaders(nx_vms_client_core "nx_vms_client_core_shaders"
    BATCHABLE
    PREFIX
        "/"
    BASE
        "static-resources"
    FILES
        ${shader_sources}
)

if(TARGET qtkeychain)
    target_link_libraries(nx_vms_client_core PRIVATE qtkeychain)
else()
    nx_exclude_sources_from_target(nx_vms_client_core
        src/nx/vms/client/core/settings/keychain_property_storage_backend)
endif()

target_include_directories(nx_vms_client_core
    PUBLIC
        ${Qt6Quick_PRIVATE_INCLUDE_DIRS}
        ${Qt6Multimedia_PRIVATE_INCLUDE_DIRS}
        ${Qt6MultimediaQuick_PRIVATE_INCLUDE_DIRS}
    PRIVATE
        ${qt_helpers_root}
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
