cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

set(customization "default" CACHE STRING "Product customization")
set(box "none" CACHE STRING "Target box")
option(beta ON "Is this build is beta build")
set(buildNumber "0" CACHE STRING "Build number")

option(analyzeMutexLocksForDeadlock OFF
    "Analyze mutex locks for deadlock. WARNING: this can significantly reduce performance!")

project(vms VERSION 3.0.0)

set(releaseVersion ${PROJECT_VERSION})
set(releaseVersion.short ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
set(releaseVersion.full ${PROJECT_VERSION}.${buildNumber})

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT WIN32)
    set(BUILD_SHARED_LIBS ON)
endif()

list(APPEND CMAKE_MODULE_PATH
    ${PROJECT_SOURCE_DIR}/build_utils/cmake
    ${PROJECT_SOURCE_DIR}/cmake
)

include(utils)
include(find_sources)
include(qrc_utils)
include(precompiled_header)
include(python_exec)
include(rdep)

detect_platform()
rdep_configure()

include(package_versions)
include(ec_protocol)
include(maven_compatibility)

include(customization/default-values.cmake)
include(customization/${customization}/customization.cmake)
include(properties)

rdep_add_package(qt)
get_full_package_name(qt QT_DIR)
set(QT_DIR ${PACKAGES_DIR}/${QT_DIR})
list(APPEND CMAKE_PREFIX_PATH ${QT_DIR})

rdep_add_package(any/boost)

find_package(Qt5
    COMPONENTS
        Core
        Gui
        Network
        Xml
        XmlPatterns
        Sql
        Concurrent
        Multimedia)

include(compiler_options)

add_subdirectory(common_libs)
add_subdirectory(common)
