## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

if("${rpiFirmware}" STREQUAL "")
    message(FATAL_ERROR "rpiFirmware not defined or empty.")
endif()

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*)
add_library(rpi_camera_plugin SHARED ${source_files})

target_compile_definitions(rpi_camera_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO}
)

target_link_libraries(rpi_camera_plugin PRIVATE
    nx_kit
    nx_sdk
    bcm_host
    openmaxil
)

target_include_directories(rpi_camera_plugin PRIVATE
    SYSTEM ${rpiFirmware}/opt/vc/include
)

target_link_directories(rpi_camera_plugin PRIVATE
    ${rpiFirmware}/opt/vc/lib
)

set_target_properties(rpi_camera_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
