## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(artifacts_dir "${open_source_root}/artifacts")

set(boost_pp_variadic_seq_src_dir "${artifacts_dir}/boost_pp_variadic_seq/src")

file(GLOB_RECURSE boost_pp_variadic_seq_files CONFIGURE_DEPENDS
    "${boost_pp_variadic_seq_src_dir}/*.h")

nx_add_target(nx_fusion LIBRARY
    PUBLIC_LIBS
        Qt6::Sql
        nx_utils
        nx_reflect
    ADDITIONAL_SOURCES
        ${boost_pp_variadic_seq_files}
    FOLDER common/libs
)

target_compile_definitions(nx_fusion
    PRIVATE NX_FUSION_API=${API_EXPORT_MACRO}
    INTERFACE NX_FUSION_API=${API_IMPORT_MACRO}
)

target_include_directories(nx_fusion
    PUBLIC
        "${boost_pp_variadic_seq_src_dir}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../artifacts/collection/src"
)

if(withTests)
    add_subdirectory(unit_tests)
endif()
