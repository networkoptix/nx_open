## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(date_time_formatter_root "${open_source_root}/artifacts/date_time_formatter")
file(GLOB_RECURSE date_time_formatter_sources
    "${date_time_formatter_root}/src/date_time_formatter/*")

set(device_vendor_names_root "${open_source_root}/artifacts/device_vendor_names")
file(GLOB_RECURSE device_vendor_names_sources
    "${device_vendor_names_root_root}/src/device_vendor_names/*")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/resources")

if(MACOSX)
    set(mac_sources src/utils/mac_utils.mm)
endif()

nx_add_target(nx_vms_common LIBRARY
    ADDITIONAL_SOURCES
        ${mac_sources}
        ${date_time_formatter_sources}
        ${device_vendor_names_sources}
    ADDITIONAL_RESOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/resources"
    PUBLIC_LIBS
        Qt6::Gui
        Qt6::Concurrent
        Qt6::Multimedia
        Qt6::Xml
        ffmpeg::ffmpeg
        nx_vms_api
        nx_vms_utils
        nx_network
        nx_network_rest
        nx_utils
        nx_sdk
        nx_fusion
        nx_sdk
        nx_rtp
        cloud_db_client
        nx_vms_update
        nx_media_core
        nx_codec
    FOLDER common/libs
)
nx_make_target_translatable(nx_vms_common COMPONENTS "client" "server" "mobile")

target_compile_definitions(nx_vms_common
    PRIVATE NX_VMS_COMMON_API=${API_EXPORT_MACRO}
    INTERFACE
        NX_VMS_COMMON_API=${API_IMPORT_MACRO}
        NX_STATISTICS_SERVER_URL="${statisticsServerUrl}"
        NX_STATISTICS_SERVER_USER="${statisticsServerUser}"
        NX_STATISTICS_SERVER_PASSWORD="${statisticsServerPassword}"
)
if(LINUX)
    add_dependencies(nx_vms_common libgl_stub libegl_stub libglx_stub libx11_stub)
endif()

target_include_directories(nx_vms_common
    PRIVATE
        ${Qt6Core_PRIVATE_INCLUDE_DIRS}
        ${device_vendor_names_root}/src
    PUBLIC
        ${date_time_formatter_root}/src
)

if(WINDOWS)
    target_link_libraries(nx_vms_common PUBLIC Winmm)
endif()

if(MACOSX)
    find_library(CoreFoundation_LIBRARY CoreFoundation)
    mark_as_advanced(CoreFoundation_LIBRARY)

    find_library(IOKit_LIBRARY IOKit)
    mark_as_advanced(IOKit_LIBRARY)

    find_library(AppKit_LIBRARY AppKit)
    mark_as_advanced(AppKit_LIBRARY)

    target_link_libraries(nx_vms_common
        PUBLIC ${CoreFoundation_LIBRARY} ${IOKit_LIBRARY} ${AppKit_LIBRARY})
endif()

if(withTests)
    add_subdirectory(test_support)
    add_subdirectory(unit_tests)
endif()
