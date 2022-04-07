## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(Qt5 COMPONENTS MultimediaQuick)

set(qt_helpers_root "${open_source_root}/artifacts/qt_helpers/src")
file(GLOB_RECURSE qt_helpers_sources "${qt_helpers_root}/qt_helpers/*")
file(GLOB_RECURSE qt_helpers_headers "${qt_helpers_root}/qt_helpers/*.h")

set(speex_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../../artifacts/speex/src")
file(GLOB speex_sources "${speex_dir}/speex/*")

nx_add_target(nx_vms_client_core LIBRARY NO_API_MACROS
    ADDITIONAL_SOURCES
        ${qt_helpers_sources}
        ${speex_sources}
    PUBLIC_LIBS
        Qt5::MultimediaQuick
        Qt5::Quick
        appserver2
        cloud_db_client
        nx_media
        nx_network
        nx_utils
        nx_vms_license
        nx_vms_rules
        nx_vms_utils
    PUBLIC_LIBS
        ffmpeg::ffmpeg
    ADDITIONAL_MOC_INCLUDE_DIRS
        ${Qt5Qml_INCLUDE_DIRS}
    ADDITIONAL_MOCABLES
        ${qt_helpers_headers}
    FOLDER client/libs
)
nx_make_target_translatable(nx_vms_client_core)

if(TARGET qtkeychain)
    target_link_libraries(nx_vms_client_core PRIVATE qtkeychain)
else()
    nx_exclude_sources_from_target(nx_vms_client_core
        src/nx/vms/client/core/settings/keychain_property_storage_backend)
endif()

target_include_directories(nx_vms_client_core
    PUBLIC
        ${Qt5Quick_PRIVATE_INCLUDE_DIRS}
        ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS}
        ${Qt5MultimediaQuick_PRIVATE_INCLUDE_DIRS}
        ${speex_dir}
    PRIVATE
        ${qt_helpers_root}
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
