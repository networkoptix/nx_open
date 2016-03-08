cmake_minimum_required(VERSION 2.8.11)

# Default configuration values. These must be before the project command or
# they won't work in Windows.
# If no build type is specified, default to "Release"
set(majorVersion 2)
set(minorVersion 6)
set(incrementalVersion 0)

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

set(qt.version 5.6.0)

set(CMAKE_PREFIX_PATH "$ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}")
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Debug Release RelWithDebInfo"
        FORCE)
endif()
# Install to "dist" directory in Windows for testing and as a staging directory
# for the installer.
if(WIN32 AND NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX dist CACHE STRING "Install path prefix.")
endif()

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${PROJECT_SOURCE_DIR}/build_utils/cmake
)

#file(STRINGS "${CMAKE_SOURCE_DIR}/customization/default-values.properties" product.name.short)
#file(STRINGS "${CMAKE_SOURCE_DIR}/customization/${customization}/build.properties" product.name.short)

include(add_module)
include(cotire)
include(find_sources)
include(add_translations)
include(process_resources)
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
#include(println)

add_module(
    NAMES
        nxtool
        common
    STATUS_SHORT_NAMES
)
