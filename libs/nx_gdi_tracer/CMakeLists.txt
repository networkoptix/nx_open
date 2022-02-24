## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_gdi_tracer LIBRARY NO_MOC NO_API_MACROS
    PUBLIC_LIBS
        detours
        stackwalker
    FOLDER client/libs
)
