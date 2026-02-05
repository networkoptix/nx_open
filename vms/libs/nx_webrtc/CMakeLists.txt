## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(libsrtp REQUIRED)
find_package(usrsctp REQUIRED)

nx_add_target(nx_webrtc LIBRARY
    PUBLIC_LIBS
        nx_vms_common
        libsrtp::libsrtp
        usrsctp::usrsctp
    FOLDER common/libs
)

target_compile_definitions(nx_webrtc
    PRIVATE NX_WEBRTC_API=${API_EXPORT_MACRO}
    INTERFACE NX_WEBRTC_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
