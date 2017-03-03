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

find_package(PythonInterp 2 REQUIRED)

detect_platform()
rdep_configure()

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

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

rdep_add_package(qt)
get_full_package_name(qt QT_DIR)
set(QT_DIR ${PACKAGES_DIR}/${QT_DIR})
list(APPEND CMAKE_PREFIX_PATH ${QT_DIR})
if(MACOSX)
    set(CMAKE_FRAMEWORK_PATH "${QT_DIR}/lib")
endif()

rdep_add_package(any/boost)
rdep_add_package(any/qtservice)
rdep_add_package(any/qtsinglecoreapplication)
rdep_add_package(any/qtsingleapplication)

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
    rdep_add_package(any/apidoctool PATH_VARIABLE APIDOCTOOL_PATH)
    add_subdirectory(mediaserver_core)
    add_subdirectory(mediaserver)
    add_subdirectory(plugins)
endif()

if(withDesktopClient OR withMobileClient)
    add_subdirectory(client)
endif()

if(withTests)
    add_subdirectory(unit_tests)
endif()

include(fix_qt_vars)
