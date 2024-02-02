## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_codec_ut
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        nx_codec
    PROJECT VMS
    COMPONENT Server
    FOLDER common/tests
)
