## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.3.2)
project(stub_analytics_plugin)

set(metadataSdkDir "" CACHE PATH "Path to unpacked VMS Metadata SDK zip.")

if(metadataSdkDir STREQUAL "")
    set(metadataSdkDir ${CMAKE_CURRENT_LIST_DIR}/../..) #< Assume building samples inside the SDK.
    if(NOT EXISTS ${metadataSdkDir}/src/nx/sdk OR NOT EXISTS ${metadataSdkDir}/nx_kit/src/nx/kit)
        message(FATAL_ERROR "Define metadataSdkDir cache variable to point to the unzipped SDK.")
    endif()
endif()

set(CMAKE_CXX_STANDARD 14)
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
file(GLOB_RECURSE SDK_SRC ${SDK_SRC_DIR}/*)

add_library(nx_sdk STATIC ${SDK_SRC})
target_include_directories(nx_sdk PUBLIC ${SDK_SRC_DIR})
target_link_libraries(nx_sdk PRIVATE nx_kit)

target_compile_definitions(nx_sdk PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}) #< for nxLibContext()

#--------------------------------------------------------------------------------------------------
# Define nx_sdk_ut executable, depends on nx_sdk.

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../unit_tests ${CMAKE_CURRENT_BINARY_DIR}/unit_tests)

#--------------------------------------------------------------------------------------------------
# Define stub_analytics_plugin lib, dynamic, depends on nx_kit and nx_sdk.

set(STUB_ANALYTICS_PLUGIN_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
file(GLOB_RECURSE STUB_ANALYTICS_PLUGIN_SRC ${STUB_ANALYTICS_PLUGIN_SRC_DIR}/*)

add_library(stub_analytics_plugin SHARED ${STUB_ANALYTICS_PLUGIN_SRC})
target_include_directories(stub_analytics_plugin PRIVATE ${STUB_ANALYTICS_PLUGIN_SRC_DIR})
target_link_libraries(stub_analytics_plugin PUBLIC nx_kit nx_sdk) #< Made PUBLIC for unit tests.

target_compile_definitions(stub_analytics_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO} #< Needed to link unit tests exe to plugin lib.
)

if(NOT WIN32)
    target_link_libraries(stub_analytics_plugin PRIVATE pthread)
endif()

#--------------------------------------------------------------------------------------------------
# Define stub_analytics_plugin_ut executable, depends on stub_analytics_plugin.

file(GLOB_RECURSE STUB_ANALYTICS_PLUGIN_UNIT_TESTS_SRC ${CMAKE_CURRENT_LIST_DIR}/unit_tests/src/*)
add_executable(stub_analytics_plugin_ut ${STUB_ANALYTICS_PLUGIN_UNIT_TESTS_SRC})
target_link_libraries(stub_analytics_plugin_ut PRIVATE stub_analytics_plugin)

add_test(NAME stub_analytics_plugin_ut COMMAND stub_analytics_plugin_ut)
