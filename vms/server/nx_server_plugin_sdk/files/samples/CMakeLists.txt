## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.14)

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "One of [Debug Release RelWithDebInfo MinSizeRel].")

project(server_sdk_samples)

set(QT_DIR "" CACHE PATH "Path to installed Qt Conan package.")
set(QT_HOST_PATH "" CACHE PATH "Path to installed Qt Host Conan package.")

set(rpiFirmware "" CACHE STRING
    "Path to Raspberry Pi libraries containing bcm_host and openmaxil.")

#--------------------------------------------------------------------------------------------------
# Set up global settings - C++ standard, rpath, find third-party libraries, etc.

if(QT_DIR AND QT_HOST_PATH)
    list(APPEND CMAKE_PREFIX_PATH ${QT_DIR})

    find_package(Qt6Core REQUIRED)
    find_package(Qt6Network REQUIRED)
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

set(CMAKE_SKIP_INSTALL_RULES TRUE)

enable_testing()

#--------------------------------------------------------------------------------------------------
# Locate nx_sdk and nx_kit sources.

set(nxSdkDir "" CACHE PATH "Path to unpacked Server Plugin SDK zip.")
if(nxSdkDir STREQUAL "")
    # Assume building samples inside the SDK.
    set(nxSdkDir ${CMAKE_CURRENT_LIST_DIR}/..)
    if(NOT EXISTS ${nxSdkDir}/src/nx/sdk OR NOT EXISTS ${nxSdkDir}/nx_kit/src/nx/kit)
        message(FATAL_ERROR "Define nxSdkDir cache variable to point to the unzipped SDK.")
    endif()
endif()

#--------------------------------------------------------------------------------------------------
# Define nx_kit lib, static.

set(nxKitLibraryType "STATIC" CACHE STRING "" FORCE)
set(nxKitWithTests "YES" CACHE STRING "" FORCE)

add_subdirectory(${nxSdkDir}/nx_kit ${CMAKE_CURRENT_BINARY_DIR}/nx_kit)

#--------------------------------------------------------------------------------------------------
# Define nx_sdk lib, static, depends on nx_kit.

set(SDK_SRC_DIR ${nxSdkDir}/src)
file(GLOB_RECURSE SDK_SRC CONFIGURE_DEPENDS ${SDK_SRC_DIR}/*)

add_library(nx_sdk STATIC ${SDK_SRC})
target_include_directories(nx_sdk PUBLIC ${SDK_SRC_DIR})
target_link_libraries(nx_sdk PRIVATE nx_kit)

target_compile_definitions(nx_sdk PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}) #< for nxLibContext()

#--------------------------------------------------------------------------------------------------
# Add nx_sdk unit tests.

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../unit_tests ${CMAKE_CURRENT_BINARY_DIR}/unit_tests)

#--------------------------------------------------------------------------------------------------
# Define samples.

add_subdirectory(analytics samples/analytics)
add_subdirectory(cloud_storage samples/cloud_storage)
add_subdirectory(device samples/device)
add_subdirectory(storage samples/storage)
