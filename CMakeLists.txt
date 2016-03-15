cmake_minimum_required(VERSION 3.0)

# Default configuration values. These must be before the project command or
# they won't work in Windows.
# If no build type is specified, default to "Release"
set(majorVersion 2)
set(minorVersion 6)
set(incrementalVersion 0)

project(vms)
set(PROJECT_SHORTNAME "vms")

# Global CMake options
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOMOC_MOC_OPTIONS "-bstdafx.h")
set(AUTOGEN_TARGETS_FOLDER ${PROJECT_BINARY_DIR}/generated)
set(CMAKE_AUTOUIC ON)

if(NOT customization)
    set(customization default)
endif()
if(NOT arch)
    set(arch x64)
endif()
if(NOT configuration)
    set(configuration release)
endif()
if(NOT beta)
    set(beta true)
endif()
if(NOT buildNumber)
    set(buildNumber 0)
endif()
if(NOT box)
    set(box none)
endif()
if(WIN32)
    set(platform "windows")
    set(additional.compiler "msvc2012u3")
    set(modification "winxp")
    set(platformToolSet "v110_xp")
    if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
        message(STATUS "Added parallel build arguments to CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
    endif()
endif()

if(WIN32)
    set(TARGET_TYPE "windows-${arch}")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    if(NOT box OR ${box} MATCHES "none")
        set(TARGET_TYPE "linux-${arch}")
    else()
        set(TARGET_TYPE ${box})
    endif()
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(TARGET_TYPE "macosx-x64")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Android")
    set(TARGET_TYPE "android-arm")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "iOS")
    set(TARGET_TYPE "ios")
endif()

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${PROJECT_SOURCE_DIR}/build_utils/cmake
    ${PROJECT_SOURCE_DIR}/customization
    ${PROJECT_SOURCE_DIR}/customization/${customization}
)

include(default_properties)
include(add_module)
include(cotire)
include(find_sources)
include(add_translations)
include(process_rc)
include(process_resources)
include(precompiled_header)
include(python_exec)
include(external_dependencies)

message("---- Building for: ${CMAKE_VS_PLATFORM_NAME}")

set (openal.version "1.16")
set (directx.version "JUN2010")
set (festival.version "2.1")
set (ffmpeg.version "0.11.4")
set (gmock.version "1.7.0")
set (gtest.version "1.7.0")
set (mfx.version "2012-R2")
set (onvif.version "2.1.2-io")
set (openssl.version "1.0.2e")
set (quazip.version "0.7.1")
set (sigar.version "1.7")
set (vmax_integration.version "2.1")
set (qt.version 5.6.0)
set (boost.version 1.56.0)

get_external_dependencies(qt boost)

if(${CMAKE_BUILD_TYPE} MATCHES Debug)
    include(${CMAKE_CURRENT_BINARY_DIR}/dependencies-debug.cmake)
else()
    include(${CMAKE_CURRENT_BINARY_DIR}/dependencies.cmake)
endif()

set(CMAKE_PREFIX_PATH ${QT_DIR})

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Debug Release RelWithDebInfo"
        FORCE)
endif()

if(NOT MSVC)
    # Enable the C++11 standard
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}
        -std=c++11
        -Werror=enum-compare
        -Werror=reorder
        -Werror=delete-non-virtual-dtor
        -Werror=return-type
        -Werror=conversion-null
        -Wuninitialized)
endif()

add_definitions(-DUSE_NX_HTTP
                -DNOMINMAX=
                -D__STDC_CONSTANT_MACROS
                -D_USING_V110_SDK71_
                -DENABLE_VMAX
                -DQN_EXPORT=
                -DUNICODE
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
                -DENABLE_COLDSTORE
                -DENABLE_MDNS
                -DENABLE_ARCHIVE
                -DENABLE_THIRD_PARTY
                -DENABLE_SOFTWARE_MOTION_DETECTION
                -DENABLE_DESKTOP_CAMERA
                -DENABLE_DATA_PROVIDERS
                -DENABLE_SENDMAIL
                -DENABLE_SSL
                -DUSE_OWN_MUTEX
                -DNX_NETWORK_API=__declspec\(dllimport\)
                -DNX_UTILS_API=__declspec\(dllimport\)
                -DUDT_API=__declspec\(dllimport\)
                )

if(WIN32)
    set(DEFAULT_LIBRARY_TYPE STATIC)
else()
    set(DEFAULT_LIBRARY_TYPE SHARED)
endif()

include(default_properties)
include(add_module)
include(cotire)
include(find_sources)
include(add_translations)
include(process_rc)
include(process_resources)
include(qt_custom)
include(precompiled_header)
include(python_exec)
include(external_dependencies)

include_directories(${BOOST_INCLUDE}
                    ${CMAKE_SOURCE_DIR}/common/src
                    $ENV{environment}/include/ffmpeg-misc-headers-win32
                    $ENV{environment}/include/glext
                    $ENV{environment}/qt5-custom
                    ${CMAKE_SOURCE_DIR}/common_libs/nx_utils/src
                    ${CMAKE_SOURCE_DIR}/common_libs/nx_utils/src/nx
                    ${CMAKE_SOURCE_DIR}/common_libs/nx_network/src
                    ${CMAKE_SOURCE_DIR}/common_libs/nx_streaming/src
                    ${CMAKE_SOURCE_DIR}/common_libs/nx_media/src
                    ${CMAKE_SOURCE_DIR}/common_libs/nx_audio/src
                    ${CMAKE_SOURCE_DIR}/common_libs/udt/src
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtZlib
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtCore/${qt.version}/QtCore
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtCore/${qt.version}
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtCore
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtQuick/${qt.version}/QtQuick
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtQuick/${qt.version}
                    $ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}/include/QtQuick
                    $ENV{environment}/artifacts/openal/${openal.version}/include
                    $ENV{environment}/artifacts/openssl/${openssl.version}/include
                    $ENV{environment}/artifacts/openal/${openal.version}/include/AL
                    $ENV{environment}/artifacts/directx/${directx.version}/include
                    $ENV{environment}/artifacts/festival/${festival.version}/include
                    $ENV{environment}/artifacts/ffmpeg/${ffmpeg.version}/include
                    $ENV{environment}/artifacts/gmock/${gmock.version}/include
                    $ENV{environment}/artifacts/gtest/${gtest.version}/include
                    $ENV{environment}/artifacts/mfx/${mfx.version}/include
                    $ENV{environment}/artifacts/onvif/${onvif.version}/include
                    $ENV{environment}/artifacts/quazip/${quazip.version}/include
                    $ENV{environment}/artifacts/sigar/${sigar.version}/include
                    $ENV{environment}/artifacts/vmax_integration/${vmax_integration.version}/include
                    )

LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/common
                $ENV{environment}/artifacts/openal/${openal.version}/${platform}/${box}/${arch}/lib/${configuration}
                $ENV{environment}/artifacts/openssl/${openssl.version}/${platform}/${box}/${arch}/lib/${configuration}
                $ENV{environment}/artifacts/directx/${directx.version}/${platform}/${box}/${arch}/lib/${configuration}
                $ENV{environment}/artifacts/festival/${festival.version}/${platform}/${box}/${arch}/lib/${configuration}
                $ENV{environment}/artifacts/ffmpeg/${ffmpeg.version}/${platform}/${box}/${arch}/lib/${configuration}
                $ENV{environment}/artifacts/gmock/${gmock.version}/${platform}/${box}/${arch}/lib/${configuration}
                $ENV{environment}/artifacts/gtest/${gtest.version}/${platform}/${box}/${arch}/lib/${configuration}
                $ENV{environment}/artifacts/mfx/${mfx.version}/${platform}/${box}/${arch}/lib/${configuration}
                $ENV{environment}/artifacts/onvif/${onvif.version}/${platform}/${box}/${arch}/lib/${configuration}
                $ENV{environment}/artifacts/quazip/${quazip.version}/${platform}/${box}/${arch}/lib/${configuration}
                $ENV{environment}/artifacts/sigar/${sigar.version}/${platform}/${box}/${arch}/lib/${configuration}
                $ENV{environment}/artifacts/vmax_integration/${vmax_integration.version}/${platform}/${box}/${arch}/lib/${configuration}
                )

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5LinguistTools)

add_module(
    NAMES
        nxtool
        common
        common_libs
        appserver2
        applauncher
    STATUS_SHORT_NAMES
)
