cmake_minimum_required(VERSION 2.8.11)

# Default configuration values. These must be before the project command or
# they won't work in Windows.
# If no build type is specified, default to "Release"

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Release Debug RelWithDebInfo" FORCE)
endif()
# Install to "dist" directory in Windows for testing and as a staging directory
# for the installer.
if (WIN32 AND NOT CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX dist CACHE STRING "Install path prefix.")
endif()
              
project(common_libs)
set(PROJECT_SHORTNAME "common_libs")
set(PROJECT_LONGNAME "${nxtool.company.name} Common Libraries")

message("${CMAKE_BINARY_DIR}/common")

add_definitions(-D_CONSOLE
                )

add_module(
    NAMES
        #nx_audio
        nx_email
        #nx_media
        #nx_network
    STATUS_SHORT_NAMES
)