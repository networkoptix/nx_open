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
#include(println)

add_module(
    NAMES
        #nxtool
        common
    STATUS_SHORT_NAMES
)
