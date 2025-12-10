## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_add_target(nx_shared_context_storage LIBRARY
    WERROR_IF LINUX
    PRIVATE_LIBS
        nx_sdk
        nx_reflect
    FOLDER common/libs
)

target_compile_definitions(nx_shared_context_storage
    PRIVATE NX_SHARED_CONTEXT_STORAGE_API=${API_EXPORT_MACRO}
    INTERFACE NX_SHARED_CONTEXT_STORAGE_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
