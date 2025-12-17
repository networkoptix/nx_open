## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_configure_file(
    build_info.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/build_info.cpp)

nx_add_target(nx_build_info LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/build_info.cpp"
    PUBLIC_LIBS
        Qt6::Core
        nx_kit nx_reflect
    FOLDER common/libs
)

target_compile_definitions(nx_build_info
    PRIVATE NX_BUILD_INFO_API=${API_EXPORT_MACRO}
    INTERFACE NX_BUILD_INFO_API=${API_IMPORT_MACRO})
