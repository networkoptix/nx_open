## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(artifacts_dir "${open_source_root}/artifacts")

find_package(RapidJSON REQUIRED)

nx_add_target(nx_reflect LIBRARY NO_MOC
    PUBLIC_LIBS
        nx_preprocessor
        Boost::headers
        RapidJSON::RapidJSON
    FOLDER common/libs
)

target_compile_definitions(nx_reflect
    PRIVATE NX_REFLECT_API=${API_EXPORT_MACRO}
    INTERFACE NX_REFLECT_API=${API_IMPORT_MACRO})

if(withTests)
    add_subdirectory(unit_tests)
endif()
