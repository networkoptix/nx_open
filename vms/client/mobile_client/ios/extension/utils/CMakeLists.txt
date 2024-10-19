## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_library(utils STATIC
    logger.mm
    helpers.mm
    push_ipc_data_impl.mm)

set(branding_file "branding.h")
set(target_branding_file ${CMAKE_CURRENT_BINARY_DIR}/${branding_file})
nx_configure_file(${branding_file}.in ${target_branding_file})

set(access_group_file "ipc_access_group.h")
set(target_access_group_file ${CMAKE_CURRENT_BINARY_DIR}/${access_group_file})
nx_configure_file(${access_group_file}.in ${target_access_group_file})

target_include_directories(utils PUBLIC "../../../src" PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
