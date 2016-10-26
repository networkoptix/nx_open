cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(vms VERSION 3.0.0)

set(release_version ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

set(customization "default" CACHE STRING "Product customization")
set(box "none" CACHE STRING "Target box")

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

include(customization/default-values.cmake)

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

add_definitions(
    -DUSE_NX_HTTP
    -D__STDC_CONSTANT_MACROS
    -DENABLE_SSL
    -DENABLE_SENDMAIL
    -DENABLE_DATA_PROVIDERS
    -DENABLE_SOFTWARE_MOTION_DETECTION
    -DENABLE_THIRD_PARTY
    -DENABLE_MDNS
)

if(WIN32)
    add_definitions(
        -DENABLE_VMAX
        -DENABLE_DESKTOP_CAMERA
    )
endif()

if(UNIX)
    add_definitions(-DQN_EXPORT=)
endif()

set(enableAllVendors ON)

if(CMAKE_SYSTEM_NAME MATCHES "Android|iOS")
    remove_definitions(
        -DENABLE_SSL
        -DENABLE_SENDMAIL
        -DENABLE_DATA_PROVIDERS
        -DENABLE_SOFTWARE_MOTION_DETECTION
        -DENABLE_THIRD_PARTY
        -DENABLE_MDNS
    )

    set(enableAllVendors OFF)
endif()

if(box MATCHES "isd.*")
    set(enableAllVendors OFF)
endif()

if(enableAllVendors)
    add_definitions(
        -DENABLE_ONVIF
        -DENABLE_AXIS
        -DENABLE_ACTI
        -DENABLE_ARECONT
        -DENABLE_DLINK
        -DENABLE_DROID
        -DENABLE_TEST_CAMERA
        -DENABLE_STARDOT
        -DENABLE_IQE
        -DENABLE_ISD
        -DENABLE_PULSE_CAMERA
    )
endif()

if(WIN32)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "")
endif()

add_subdirectory(common_libs)
add_subdirectory(common)

# set(CMAKE_AUTOMOC_MOC_OPTIONS "-bstdafx.h")
#
# if(WIN32)
#     set(platform "windows")
#     set(additional.compiler "msvc2012u3")
#     set(modification "winxp")
#     set(platformToolSet "v110_xp")
#     if(MSVC)
#         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
#         message(STATUS "Added parallel build arguments to CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
#     endif()
# endif()
#
# if(NOT MSVC)
#     # Enable the C++11 standard
#     set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}
#         -std=c++11
#         -Werror=enum-compare
#         -Werror=reorder
#         -Werror=delete-non-virtual-dtor
#         -Werror=return-type
#         -Werror=conversion-null
#         -Wuninitialized)
# endif()
