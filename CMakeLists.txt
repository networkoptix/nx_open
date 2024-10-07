## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.20.0 FATAL_ERROR) #< This version is bundled with MSVC 2019.

set(open_source_root "${CMAKE_SOURCE_DIR}")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(cmake/find_python.cmake)
find_package(PythonInterp)

include(cmake/open_source/defaults.cmake)
include(cmake/customization_package.cmake)

include(cmake/default_target.cmake)
set(targetDevice ${default_target_device} CACHE STRING
    "Target device. For the list see cmake/toolchains.")
if(NOT clangDir STREQUAL "")
    set(ENV{CLANG_DIR} ${clangDir})
endif()
include(cmake/open_source/toolchain_detection.cmake)

include(cmake/option.cmake)
nx_option(developerBuild
    "Disable this option for production builds"
    ON)

if(developerBuild)
    set(default_cmake_build_type "Debug")
else()
    set(default_cmake_build_type "Release")
endif()
set(CMAKE_BUILD_TYPE ${default_cmake_build_type} CACHE STRING
    "One of [Debug Release RelWithDebInfo MinSizeRel].")

include(cmake/distribution/flavors.cmake)
nx_init_distribution_flavor_list()

include(cmake/utils.cmake)
nx_init_known_files_list()
include(cmake/ninja_tool.cmake)

# "AUTO" is used to autodetect the build number from git.
set(buildNumber "AUTO" CACHE STRING "Build number")
include(cmake/vcs_helpers.cmake)
if(buildNumber STREQUAL "AUTO")
    nx_vcs_get_info(
        CHANGESET changeSet BRANCH branch CURRENT_REFS currentRefs BUILD_NUMBER buildNumber
    )
    if(buildNumber STREQUAL "0")
        message(WARNING "ATTENTION: No compatible Server version found. Setting buildNumber to 0.")
    endif()
else()
    nx_vcs_get_info(CHANGESET changeSet BRANCH branch CURRENT_REFS currentRefs)
endif()
if(NOT changeSet OR NOT branch OR NOT currentRefs)
    message(FATAL_ERROR "VCS info cannot be read.")
endif()

include(cmake/versions/release_version.cmake)

set(conanNxRemote https://artifactory.nxvms.dev/artifactory/api/conan/conan
    CACHE INTERNAL "The URL of Conan remote containing packages needed for the build."
)
include(cmake/conan_dependencies.cmake)

project(vms_opensource VERSION ${releaseVersion})

include(cmake/versions/meta_version.cmake)
include(cmake/versions/nx_sdk_version.cmake)
include(cmake/versions/vms_protocol_version.cmake)

list(APPEND CMAKE_MODULE_PATH
    ${CMAKE_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/cmake
)

include(target_detection)
include(cmake/open_source/vms_parts.cmake)
include(output_directories)
include(cmake/dependencies.cmake)

find_package(Boost REQUIRED COMPONENTS headers)
find_package(ffmpeg REQUIRED)
find_package(OpenSSL REQUIRED)

include(qt_utils)
include(cpp_utils)
include(process_target_debug_symbols)
include(find_sources)
include(compiler_options)
include(linker_options)
nx_load_customization_package()
include(distribution_names)
include(cmake/properties.cmake)
include(cmake/cloud_host.cmake)
include(generate_open_source_config)
include(project_utils)
include(test_utils)

if(WINDOWS AND withDistributions)
    include(cmake/distribution/wix.cmake)
    nx_wix_generate_localization_file()
endif()

add_subdirectory(cloud)
add_subdirectory(dependencies)
add_subdirectory(libs)

include(cmake/distribution/common.cmake)
add_subdirectory(vms)

if(WINDOWS)
    include(windows_utils)
    nx_prepare_vs_settings()
endif()

nx_generate_compatible_servers_txt()

nx_create_dev_qt_conf()

nx_save_known_files()
nx_save_ninja_preprocessor_script()
