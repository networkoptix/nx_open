## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_reflect_ut
    NO_NX_UTILS
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        nx_reflect Qt6::Core
    PROJECT NXLIB
    FOLDER common/tests
)
