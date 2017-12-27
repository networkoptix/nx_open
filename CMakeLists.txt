cmake_minimum_required(VERSION 3.8.0 FATAL_ERROR)

set(customization "default" CACHE STRING "Product customization")
option(beta "Is this build is beta build" ON)
set(buildNumber "0" CACHE STRING "Build number")

option(analyzeMutexLocksForDeadlock
    "Analyze mutex locks for deadlock. WARNING: this can significantly reduce performance!"
    OFF)

option(rdepSync
    "Whether rdep should sync packages or use only existing copies"
    ON)

option(addQtPdb
    "Whether add Qt PDB files to debug libs"
    OFF)

set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
    "One of [Debug Release RelWithDebInfo MinSizeRel].")

set(targetDevice "" CACHE STRING "Target device. For the list see cmake/toolchains.")
set(box "none")

if(targetDevice STREQUAL "none")
    set(targetDevice "")
endif()

# Early loading rdep to download a toolchain if needed.
include(build_utils/cmake/find_python.cmake)
include(build_utils/cmake/rdep.cmake)
nx_rdep_configure()

if(targetDevice AND NOT targetDevice STREQUAL "ios")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain/${targetDevice}.cmake")
    include(cmake/sync_toolchain.cmake)
endif()

project(vms VERSION 3.1.0)

if(targetDevice STREQUAL "ios")
    include(cmake/ios.cmake)
endif()

set(releaseVersion ${PROJECT_VERSION})
set(releaseVersion.short ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
set(releaseVersion.full ${releaseVersion}.${buildNumber})

set(mobileClientVersion 17.2)
set(mobileClientVersion.full ${mobileClientVersion}.${buildNumber})

set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND CMAKE_MODULE_PATH
    ${PROJECT_SOURCE_DIR}/build_utils/cmake
    ${PROJECT_SOURCE_DIR}/cmake)

include(CMakeParseArguments)
include(ExternalProject)
include(target_detection)
include(utils)
include(find_sources)
include(qrc_utils)
include(precompiled_header)
include(strip_target)
include(mercurial)
include(project_utils)
include(test_utils)

include(ec_protocol)

set(customization_dir "${PROJECT_SOURCE_DIR}/customization/${customization}")

include(customization/default-values.cmake)
include(${customization_dir}/customization.cmake)
include(properties)
include(cloud_host)
include(vms_parts)
include(compiler_options)
include(output_directories)
include(distribution_names)

if(NOT (WINDOWS OR ANDROID OR IOS))
    set(BUILD_SHARED_LIBS ON)
endif()

include(dependencies)
include(maven_compatibility)
include(generate_current_config)

nx_create_dev_qt_conf()

list(APPEND CMAKE_PREFIX_PATH ${QT_DIR})
if(MACOSX)
    set(CMAKE_FRAMEWORK_PATH "${QT_DIR}/lib")
endif()
if(LINUX)
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,${QT_DIR}/lib")
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath-link,${QT_DIR}/lib")
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    set(CMAKE_SHARED_LINKER_FLAGS
        "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath-link,${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()

if(ANDROID)
    include(android-apk)
elseif(IOS)
    include(ios_app)
endif()

link_directories("${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

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

if(withDesktopClient OR withMobileClient OR withNxTool)
    find_package(Qt5
        COMPONENTS
            Qml
            Quick)
endif()

set(QML_IMPORT_PATH
    "${QT_DIR}/qml"
    "${CMAKE_CURRENT_LIST_DIR}/client/mobile_client/static-resources/qml"
    "${CMAKE_CURRENT_LIST_DIR}/nxtool/static-resources/src/qml"
    CACHE STRING "Extra QML import paths")

if(ANDROID)
    find_package(Qt5 COMPONENTS AndroidExtras)
endif()

if(WINDOWS)
    # Prepare a file to be included into each Visual Studio project.
    nx_configure_file(${PROJECT_SOURCE_DIR}/msvc.user.props ${CMAKE_BINARY_DIR})
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_subdirectory(wixsetup)
    endif()
endif()

# Required for QN_DECLARE_METAOBJECT_HEADER
include_directories("${PROJECT_SOURCE_DIR}/common_libs/nx_fusion/src")

add_subdirectory(common_libs)
add_subdirectory(common)
add_subdirectory(appserver2)
add_subdirectory(nx_cloud)

if(withMediaServer)
    add_subdirectory(mediaserver_core)
    add_subdirectory(mediaserver)
    add_subdirectory(plugins)
    add_subdirectory(storage_plugins)
endif()

if(withTestCamera)
    add_subdirectory(testcamera)
endif()

if(withTrayTool)
    add_subdirectory(traytool)
endif()

if(withDesktopClient OR withMobileClient)
    add_subdirectory(client)
endif()

if(withNxTool)
    add_subdirectory(nxtool)
endif()

if(withTests)
    add_subdirectory(unit_tests)
    add_subdirectory(tests)
endif()

if(targetDevice MATCHES "bpi|bananapi|rpi|edge1")
    add_subdirectory(edge_firmware)
endif()

include(fix_qt_vars)

nx_configure_file(
    build_variables/maven/filter-resources/build_info.txt
    ${CMAKE_CURRENT_BINARY_DIR})
