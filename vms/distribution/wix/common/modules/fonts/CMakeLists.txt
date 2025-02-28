## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(wix_utils)

nx_wix_heat(${CMAKE_CURRENT_BINARY_DIR}/fonts.wixobj
    SOURCE_DIR ${CMAKE_BINARY_DIR}/bin/fonts
    TARGET_DIR_ALIAS FontsDirectoryReference
    COMPONENT_GROUP FontsComponentGroup)

nx_wix_candle(fonts_module)

nx_wix_add_light_target(distribution_wix_fonts_module
    ${common_modules_output_path}/fonts.msm
    SOURCE_FILES
        ${CMAKE_CURRENT_BINARY_DIR}/fonts.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/fonts_module.wixobj)
