## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_test(nx_pathkit_ut
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        nx_pathkit
    PROJECT NXLIB
    FOLDER common/tests
)

target_include_directories(nx_pathkit_ut
    PRIVATE
        ${Qt6Gui_PRIVATE_INCLUDE_DIRS})
