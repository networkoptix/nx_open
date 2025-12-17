## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_zip LIBRARY
    PUBLIC_LIBS
        Qt6::Core
        quazip
        nx_utils
    FOLDER common/libs
)

target_compile_definitions(nx_zip
    PRIVATE NX_ZIP_API=${API_EXPORT_MACRO}
    INTERFACE NX_ZIP_API=${API_IMPORT_MACRO})
