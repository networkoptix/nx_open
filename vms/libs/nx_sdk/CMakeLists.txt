## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/


# nx_sdk is a static lib because it needs a dedicated instance of its context (global variables) in
# each Plugin and in the Server. Also, only one dynamic library of the Server can link with nx_sdk.

# Substitute nxSdkVersion into nx_sdk_version.inc to be included in nx/sdk/helpers/lib_context.cpp.
set(copied_nx_sdk_version_origin ${CMAKE_CURRENT_BINARY_DIR})
set(copied_nx_sdk_version_file ${copied_nx_sdk_version_origin}/nx_sdk_version.inc)
nx_configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/nx_sdk_version.inc.in
    ${copied_nx_sdk_version_file} @ONLY NEWLINE_STYLE UNIX)

nx_add_target(nx_sdk LIBRARY LIBRARY_TYPE STATIC NO_MOC NO_PCH
    ADDITIONAL_SOURCES
        ${copied_nx_sdk_version_file}
    PUBLIC_LIBS
        nx_kit
    FOLDER server/libs
)

target_compile_definitions(nx_sdk
    PRIVATE NX_SDK_API=${API_EXPORT_MACRO} #< Used to export nxPluginContext().
    INTERFACE NX_SDK_API= #< No need to import nxPluginContext() - called only by name in runtime.
)
target_include_directories(nx_sdk PRIVATE ${copied_nx_sdk_version_origin})

if(withTests)
    add_subdirectory(unit_tests)

    # Actually this should be in the unit_tests/CMakeLists.txt, but that file is used as a template
    # for the auto-generated CMakeLists.txt in the sample archive.
    nx_set_test_meta_info(nx_sdk_ut PROJECT NXLIB)
    add_dependencies(unit_tests nx_sdk_ut)
endif()
