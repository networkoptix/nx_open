## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(vms_gateway_core LIBRARY NO_MOC
    PUBLIC_LIBS
        nx_vms_common
        nx_network
        nx_fusion
        nx_utils
    FOLDER client/libs
)

set(mocableNetworkFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/cloud/vms_gateway/vms_gateway_embeddable.h)

nx_add_qt_mocables(vms_gateway_core ${mocableNetworkFiles}
    INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(vms_gateway_core
    PRIVATE NX_VMS_GATEWAY_API=${API_EXPORT_MACRO}
    INTERFACE NX_VMS_GATEWAY_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
