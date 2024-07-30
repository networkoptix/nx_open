## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.14)
project(analytics_plugin_ut)

# Unit test which tests an arbitrary list of Analytics Plugins.
#
# The list of plugin dynamic libraries must be specified in analytics_plugin_ut.cfg located next
# to the unit test executable.
#
# NOTE: To simplify the build system, unit tests of nx_kit and the SDK core library are built and
# run in the scope of this project.

set(metadataSdkDir "" CACHE PATH "Path to unpacked VMS Metadata SDK zip.")
if(metadataSdkDir STREQUAL "")
    set(metadataSdkDir ${CMAKE_CURRENT_LIST_DIR}/../..) #< Assume building the test inside the SDK.
    if(NOT EXISTS ${metadataSdkDir}/src/nx/sdk OR NOT EXISTS ${metadataSdkDir}/nx_kit/src/nx/kit)
        message(FATAL_ERROR "Define metadataSdkDir cache variable to point to the unzipped SDK.")
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(UNIX)
    # In Linux, for the plugin .so library, set `rpath` to "$ORIGIN" and do not set `runpath`, thus
    # enabling the lookup of the dependencies in the plugin dir first.
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--disable-new-dtags")
endif()
set(CMAKE_SKIP_BUILD_RPATH ON)
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN")

# Use all CPU cores by MSVC.
if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /MP")
    # Do not create separate .pdb files for object files (workaround for mspdbsrv.exe bug, needed
    # for using "/MP" flag).
    add_compile_options("/Z7")
endif()

if(WIN32)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

enable_testing()

#--------------------------------------------------------------------------------------------------
# Define nx_kit lib, static.

set(nxKitLibraryType "STATIC" CACHE STRING "" FORCE)
set(nxKitWithTests "YES" CACHE STRING "" FORCE)

add_subdirectory(${metadataSdkDir}/nx_kit ${CMAKE_CURRENT_BINARY_DIR}/nx_kit)

#--------------------------------------------------------------------------------------------------
# Define nx_sdk lib, static, depends on nx_kit.

set(SDK_SRC_DIR ${metadataSdkDir}/src)
file(GLOB_RECURSE SDK_SRC CONFIGURE_DEPENDS ${SDK_SRC_DIR}/*)

add_library(nx_sdk STATIC ${SDK_SRC})
target_include_directories(nx_sdk PUBLIC ${SDK_SRC_DIR})
target_link_libraries(nx_sdk PRIVATE nx_kit)

target_compile_definitions(nx_sdk PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}) #< for nxLibContext()

#--------------------------------------------------------------------------------------------------
# Define nx_sdk_ut executable, depends on nx_sdk.

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../unit_tests ${CMAKE_CURRENT_BINARY_DIR}/unit_tests)

#--------------------------------------------------------------------------------------------------
# Define analytics_plugin_ut executable.

file(GLOB_RECURSE ANALYTICS_PLUGIN_UNIT_TESTS_SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/src/*)

add_executable(analytics_plugin_ut ${ANALYTICS_PLUGIN_UNIT_TESTS_SRC})

target_link_libraries(analytics_plugin_ut PRIVATE nx_kit nx_sdk)
if(NOT WIN32)
    target_link_libraries(analytics_plugin_ut PRIVATE dl)
    set_target_properties(analytics_plugin_ut PROPERTIES LINK_FLAGS -pthread)
endif()

target_compile_definitions(analytics_plugin_ut PRIVATE NX_SDK_API=) #< for nxLibContext()

add_test(NAME analytics_plugin_ut COMMAND analytics_plugin_ut)

# Generate a config file with the plugin library list.

set(testConfigFileHeader "# Paths to plugin library files which must be unit-tested.")

if(WIN32)
    set(libPrefix "")
    set(libSuffix "dll")
else()
    set(libPrefix "lib")
    set(libSuffix "so")
endif()

set(testConfigFile ${CMAKE_CURRENT_BINARY_DIR}/analytics_plugin_ut.cfg)
string(CONCAT testConfigContent
    "${testConfigFileHeader}\n"
    "../stub_analytics_plugin/${libPrefix}stub_analytics_plugin.${libSuffix}\n"
    "../sample_analytics_plugin/${libPrefix}sample_analytics_plugin.${libSuffix}\n"
)
file(GENERATE OUTPUT ${testConfigFile} CONTENT ${testConfigContent})
