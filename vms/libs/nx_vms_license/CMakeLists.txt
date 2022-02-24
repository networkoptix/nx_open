## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_vms_license LIBRARY NO_API_MACROS LIBRARY_TYPE STATIC
    PUBLIC_LIBS nx_vms_common
    FOLDER common/libs
)

if(withTests AND (withMediaServer OR withClouds OR openSourceBuild))
    add_subdirectory(unit_tests)
endif()
