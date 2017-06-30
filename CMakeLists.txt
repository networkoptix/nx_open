cmake_minimum_required(VERSION 3.8.0 FATAL_ERROR)

set(customization "default" CACHE STRING "Product customization")
option(beta "Is this build is beta build" ON)
set(buildNumber "0" CACHE STRING "Build number")

option(analyzeMutexLocksForDeadlock
    "Analyze mutex locks for deadlock. WARNING: this can significantly reduce performance!"
    OFF)

#option(rdepSync
#    "Whether rdep should sync packages or use only existing copies"
#    ON)

set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
    "One of [Debug Release RelWithDebInfo MinSizeRel].")

set(targetDevice "" CACHE STRING "Target device. For the list see cmake/toolchains.")
set(box "none")

# Early loading rdep to download a toolchain if needed.
include(build_utils/cmake/find_python.cmake)
include(build_utils/cmake/rdep.cmake)
#nx_rdep_configure()

if(targetDevice)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain/${targetDevice}.cmake")
    include(cmake/sync_toolchain.cmake)
endif()

project(vms VERSION 3.1.0)

set(releaseVersion ${PROJECT_VERSION})
set(releaseVersion.short ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
set(releaseVersion.full ${releaseVersion}.${buildNumber})

set(mobileClientVersion 17.2)
set(mobileClientVersion.full ${mobileClientVersion}.${buildNumber})

set (VCVars "VS140COMNTOOLS")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

list(APPEND CMAKE_MODULE_PATH
    ${PROJECT_SOURCE_DIR}/build_utils/cmake
    ${PROJECT_SOURCE_DIR}/cmake)

include(CMakeParseArguments)
include(target_detection)
include(utils)
include(find_sources)
include(qrc_utils)
include(precompiled_header)
include(mercurial)
include(project_utils)
include(test_utils)

include(ec_protocol)
include(maven_compatibility)

set(customization_dir "${PROJECT_SOURCE_DIR}/customization/${customization}")

include(customization/default-values.cmake)
include(${customization_dir}/customization.cmake)
include(properties)
include(cloud_host)
#include(vms_parts)
include(compiler_options)
include(output_directories)
include(distribution_names)

set(root.dir "${PROJECT_SOURCE_DIR}")
set(artifact.name.product ${installer.name})
set(artifact.name.prefix ${artifact.name.product})
set(artifact.name.suffix ${artifact.name.version}-${artifact.name.platform}${artifact.name.beta.suffix}${artifact.name.cloud.suffix})
# List of public final names which are to be used everywhere
set(artifact.name.client ${artifact.name.prefix}-client-${artifact.name.suffix})
set(artifact.name.server ${artifact.name.prefix}-server-${artifact.name.suffix})
set(artifact.name.bundle ${artifact.name.prefix}-bundle-${artifact.name.suffix})
set(artifact.name.servertool ${artifact.name.prefix}-servertool-${artifact.name.suffix})
set(artifact.name.client_update ${artifact.name.prefix}-client_update-${artifact.name.suffix})
set(artifact.name.server_update ${artifact.name.prefix}-server_update-${artifact.name.suffix})
set(artifact.name.paxton ${artifact.name.prefix}-paxton-${artifact.name.suffix})
set(artifact.name.paxton_plugin ${artifact.name.prefix}-paxton_plugin-${artifact.name.suffix})
set(artifact.name.cdb ${artifact.name.prefix}-cdb-${artifact.name.suffix})
set(artifact.name.hpm ${artifact.name.prefix}-hpm-${artifact.name.suffix})
set(artifact.name.client_debug ${artifact.name.prefix}-client_debug-${artifact.name.suffix})
set(artifact.name.server_debug ${artifact.name.prefix}-server_debug-${artifact.name.suffix})
set(artifact.name.misc_debug ${artifact.name.prefix}-misc_debug-${artifact.name.suffix})
set(artifact.name.libs_debug ${artifact.name.prefix}-libs_debug-${artifact.name.suffix})
set(artifact.name.cloud_debug ${artifact.name.prefix}-cloud_debug-${artifact.name.suffix})
set(artifact.name.testcamera ${artifact.name.prefix}-testcamera-${artifact.name.suffix})
set(environment.dir $ENV{environment})

if(NOT (WINDOWS OR ANDROID OR IOS))
    set(BUILD_SHARED_LIBS ON)
endif()

include(dependencies)

if(CMAKE_CROSSCOMPILING)
    set(QT_PREFIX "..")
else()
    set(QT_PREFIX "${QT_DIR}")
    file(TO_CMAKE_PATH "${QT_PREFIX}" QT_PREFIX)
endif()
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

set(qt.dir ${QT_DIR})

if(ANDROID)
    include(android-apk)
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

if(withDesktopClient OR withMobileClient)
    find_package(Qt5
        COMPONENTS
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

if(WINDOWS)
    # Prepare a file to be included into each Visual Studio project.
    nx_configure_file(${PROJECT_SOURCE_DIR}/msvc.user.props ${CMAKE_BINARY_DIR})
	add_subdirectory(wixsetup)
endif()

# Required for QN_DECLARE_METAOBJECT_HEADER
#include_directories("${PROJECT_SOURCE_DIR}/common_libs/nx_fusion/src")

#add_subdirectory(common_libs)
#add_subdirectory(common)
#add_subdirectory(appserver2)
#add_subdirectory(nx_cloud)

if(withMediaServer)
#    add_subdirectory(mediaserver_core)
#    add_subdirectory(mediaserver)
#    add_subdirectory(plugins)
#    add_subdirectory(storage_plugins)
endif()

if(withDesktopClient OR withMobileClient)
#    add_subdirectory(client)
endif()

if(withTests)
#    add_subdirectory(unit_tests)
#    add_subdirectory(tests)
endif()

include(fix_qt_vars)
