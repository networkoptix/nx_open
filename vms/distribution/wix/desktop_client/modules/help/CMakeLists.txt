## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(wix_utils)

nx_wix_heat(${CMAKE_CURRENT_BINARY_DIR}/client_help.wixobj
    SOURCE_DIR ${help_directory}/help
    TARGET_DIR_ALIAS HelpDirectoryReference
    COMPONENT_GROUP HelpComponentGroup)

nx_wix_candle(client_help_module)

nx_wix_add_light_target(distribution_wix_client_help_module
    ${modules_output_path}/client_help.msm
    SOURCE_FILES
        ${CMAKE_CURRENT_BINARY_DIR}/client_help.wixobj
        ${CMAKE_CURRENT_BINARY_DIR}/client_help_module.wixobj)
