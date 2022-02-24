## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(artifacts_dir "${open_source_root}/artifacts")

set(rapidjson_src_dir "${artifacts_dir}/rapidjson/src")

file(GLOB_RECURSE rapidjson_files CONFIGURE_DEPENDS "${rapidjson_src_dir}/*.h")

nx_add_target(nx_reflect LIBRARY NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS
        nx_preprocessor
        Boost::headers
    ADDITIONAL_SOURCES
        ${rapidjson_files}
    FOLDER common/libs
)

target_compile_definitions(nx_reflect
    PRIVATE NX_REFLECT_API=${API_EXPORT_MACRO}
    INTERFACE NX_REFLECT_API=${API_IMPORT_MACRO})

# Use "SYSTEM" option here to supress warnings coming out of artifact headers.
target_include_directories(nx_reflect SYSTEM PUBLIC "${rapidjson_src_dir}")

if(withTests)
    add_subdirectory(unit_tests)
endif()
