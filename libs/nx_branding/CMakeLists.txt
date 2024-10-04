## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_configure_file(
    branding.cpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/branding.cpp)

nx_add_target(nx_branding LIBRARY
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_BINARY_DIR}/branding.cpp"
    PUBLIC_LIBS
        Qt6::Core
    FOLDER common/libs
)

target_compile_definitions(nx_branding
    PRIVATE NX_BRANDING_API=${API_EXPORT_MACRO}
    INTERFACE NX_BRANDING_API=${API_IMPORT_MACRO})
