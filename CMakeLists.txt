cmake_minimum_required(VERSION 3.7.0 FATAL_ERROR)

set(customization "default" CACHE STRING "Product customization")
set(box "none" CACHE STRING "Target box")
option(beta "Is this build is beta build" ON)
set(buildNumber "0" CACHE STRING "Build number")

option(analyzeMutexLocksForDeadlock
    "Analyze mutex locks for deadlock. WARNING: this can significantly reduce performance!"
    OFF)

option(rdepSync
    "Whether rdep should sync packages or use only existing copies"
    ON)

set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
    "One of [Debug Release RelWithDebInfo MinSizeRel].")

project(vms VERSION 3.0.0)

set(releaseVersion ${PROJECT_VERSION})
set(releaseVersion.short ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
set(releaseVersion.full ${PROJECT_VERSION}.${buildNumber})

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT WIN32 AND NOT ANDROID AND NOT IOS)
    set(BUILD_SHARED_LIBS ON)
endif()

list(APPEND CMAKE_MODULE_PATH
    ${PROJECT_SOURCE_DIR}/build_utils/cmake
    ${PROJECT_SOURCE_DIR}/cmake)

include(CMakeParseArguments)
include(utils)
include(find_sources)
include(qrc_utils)
include(precompiled_header)
include(rdep)
include(mercurial)
include(project_utils)

find_python()
detect_platform()
nx_rdep_configure()

include(android-ndk-workaround)

include(package_versions)
include(ec_protocol)
include(maven_compatibility)

set(customization_dir "${PROJECT_SOURCE_DIR}/customization/${customization}")

include(customization/default-values.cmake)
include(${customization_dir}/customization.cmake)
include(properties)
include(cloud_host)
include(vms_parts)
include(compiler_options)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

nx_rdep_add_package(qt PATH_VARIABLE QT_DIR)
if(CMAKE_CROSSCOMPILING)
    set(QT_PREFIX "..")
else()
    set(QT_PREFIX "${QT_DIR}")
endif()
list(APPEND CMAKE_PREFIX_PATH ${QT_DIR})
if(MACOSX)
    set(CMAKE_FRAMEWORK_PATH "${QT_DIR}/lib")
endif()
if(NOT WIN32)
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,${QT_DIR}/lib")
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath-link,${QT_DIR}/lib")
endif()

nx_rdep_add_package(any/boost)
nx_rdep_add_package(any/qtservice)
nx_rdep_add_package(any/qtsinglecoreapplication)
nx_rdep_add_package(any/qtsingleapplication)

find_package(Qt5
    COMPONENTS
        LinguistTools
        Core
        Gui
        Network
        Xml
        XmlPatterns
        Sql
        Concurrent
        Multimedia)

if(withDesktopClient OR withMobileClient)
    find_package(Qt5
        COMPONENTS
            OpenGL
            Qml
            Quick)
endif()

set(QML_IMPORT_PATH
    "${QT_DIR}/qml"
    "${CMAKE_CURRENT_LIST_DIR}/client/mobile_client/static-resources/qml"
    CACHE STRING "Extra QML import paths")

if(ANDROID)
    find_package(Qt5 COMPONENTS AndroidExtras)
endif()

# Required for QN_DECLARE_METAOBJECT_HEADER
include_directories("${PROJECT_SOURCE_DIR}/common_libs/nx_fusion/src")

add_subdirectory(common_libs)
add_subdirectory(common)
add_subdirectory(appserver2)
add_subdirectory(nx_cloud)

if(withMediaServer)
    nx_rdep_add_package(any/apidoctool PATH_VARIABLE APIDOCTOOL_PATH)
    add_subdirectory(mediaserver_core)
    add_subdirectory(mediaserver)
    add_subdirectory(plugins)
    add_subdirectory(storage_plugins)
endif()

if(withDesktopClient OR withMobileClient)
    add_subdirectory(client)
endif()

if(withTests)
    nx_rdep_add_package(gtest)
    nx_rdep_add_package(gmock)
    add_subdirectory(unit_tests)
endif()

include(fix_qt_vars)
