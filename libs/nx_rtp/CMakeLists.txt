## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_rtp
    LIBRARY
    PUBLIC_LIBS
        nx_utils
        nx_media_core
        Qt6::Network
    FOLDER common/libs
)

target_compile_definitions(nx_rtp
    PRIVATE NX_RTP_API=${API_EXPORT_MACRO}
    INTERFACE NX_RTP_API=${API_IMPORT_MACRO}
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
