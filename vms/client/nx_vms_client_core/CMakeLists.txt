## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(Qt6 COMPONENTS Multimedia ShaderTools Test REQUIRED)

set(qt_helpers_root "${open_source_root}/artifacts/qt_helpers/src")
file(GLOB_RECURSE qt_helpers_sources "${qt_helpers_root}/qt_helpers/*")
file(GLOB_RECURSE qt_helpers_headers "${qt_helpers_root}/qt_helpers/*.h")

set(nx_vms_client_core_external_resources_file
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/client_core_external.dat)
nx_add_external_resources_target(nx_vms_client_core_external_resources
    TARGET_FILE
        ${nx_vms_client_core_external_resources_file}
    QRC_FILE
        client_core_external
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/external_resources
)

set(bytedance_iconpark_file ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bytedance_iconpark.dat)
nx_add_external_resources_target(bytedance_iconpark
    TARGET_FILE
        ${bytedance_iconpark_file}
    QRC_FILE
        bytedance_iconpark
    FILES
        ${open_source_root}/artifacts/analytics_icons/bytedance_iconpark
)

nx_add_target(nx_vms_client_core LIBRARY
    ADDITIONAL_SOURCES
        ${qt_helpers_sources}
    PUBLIC_LIBS
        Qt6::Multimedia
        Qt6::Quick
        Qt6::Svg
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
add_dependencies(nx_vms_client_core nx_vms_client_core_external_resources)
add_dependencies(nx_vms_client_core bytedance_iconpark)

target_compile_definitions(nx_vms_client_core
    PRIVATE NX_VMS_CLIENT_CORE_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_CLIENT_CORE_API=${API_IMPORT_MACRO})

file(GLOB_RECURSE shader_sources CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "static-resources/qml/*.vert"
    "static-resources/qml/*.frag"
)
# This command generates nx_vms_client_core_other_files target.
qt6_add_shaders(nx_vms_client_core "nx_vms_client_core_shaders"
    BATCHABLE
    PREFIX
        "/"
    BASE
        "static-resources"
    FILES
        ${shader_sources}
)
set_property(TARGET nx_vms_client_core_other_files PROPERTY FOLDER client/other_files)

# Workaround for Qt bug with Xcode new build system:
#
# The custom command generating
#
#   open/vms/client/nx_vms_client_core/.qsb/qml/Nx/Core/Items/MaskedUniformGrid.frag.qsb
#
# is attached to multiple targets:
#
#   nx_vms_client_core_other_files
#   nx_vms_client_core_resources_1
#
# but none of these is a common dependency of the other(s).  This is not
# allowed by the Xcode "new build system".
#
# Qt tries to expose source files to IDE using fake target ${target}_other_files.
if(CMAKE_GENERATOR STREQUAL "Xcode")
    set_property(TARGET nx_vms_client_core_other_files PROPERTY SOURCES "")
endif()

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
