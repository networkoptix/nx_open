###
# Copyright (c) 2009, Paul Gideon Dann
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
###

cmake_minimum_required(VERSION 2.8.11)

# Default configuration values. These must be before the project command or
# they won't work in Windows.
# If no build type is specified, default to "Release"

#set (CMAKE_PREFIX_PATH "$ENV{environment}/artifacts/qt/${qt.version}/${platform}/${arch}/${box}")
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
      "Release Debug RelWithDebInfo"
      FORCE)
endif()
# Install to "dist" directory in Windows for testing and as a staging directory
# for the installer.
if (WIN32 AND NOT CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX dist CACHE STRING "Install path prefix.")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/defines.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/defines.h)

include(default_properties)
               
project(nxtool)
set(PROJECT_SHORTNAME "nxtool")
set(PROJECT_LONGNAME "${nxtool.company.name} Server Tool")
set(PROJECT_VERSION "2.5.0")

include(default_project_properties)

# Global CMake options
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

#file(STRINGS "${CMAKE_SOURCE_DIR}/customization/default-values.properties" product.name.short)
#file(STRINGS "${CMAKE_SOURCE_DIR}/customization/${customization}/build.properties" product.name.short)

# Additional Include Directories
include_directories (src ${CMAKE_SOURCE_DIR}/common/src $ENV{environment}/include $ENV{environment}/include/boost)

if (NOT SKIP_GEN)
  file (GLOB_RECURSE FILTERED_FILES "${PROJECT_SOURCE_DIR}/resources/filter/*")
  foreach(filename ${FILTERED_FILES})
    string (REPLACE "${PROJECT_SOURCE_DIR}/resources/filter/" "" filename ${filename})
    configure_file(${PROJECT_SOURCE_DIR}/resources/filter/${filename}
                   ${CMAKE_CURRENT_BINARY_DIR}/${filename})
    message ("Filtering and Copying: " ${filename})
  endforeach(filename ${FILTERED_FILES})

  file (GLOB_RECURSE COMMON_FILTERED_FILES "${CMAKE_SOURCE_DIR}/cpp/scripts/*") 
  foreach(filename ${COMMON_FILTERED_FILES})
    string (REPLACE "${CMAKE_SOURCE_DIR}/cpp/scripts/" "" filename ${filename})
    configure_file(${CMAKE_SOURCE_DIR}/cpp/scripts/${filename}
                   ${CMAKE_CURRENT_BINARY_DIR}/${filename})
    message ("Filtering and Copying: " ${filename})
  endforeach(filename ${COMMON_FILTERED_FILES})  
  
  #configure_file(${CMAKE_SOURCE_DIR}/cpp/scripts/gen_resources.py
  #               ${CMAKE_CURRENT_BINARY_DIR}/gen_resources.py) 
  #file(GLOB RESOURCES "${PROJECT_SOURCE_DIR}/resources/filter/*")
  file(COPY ${RESOURCES} DESTINATION ${PROJECT_BINARY_DIR})
  execute_process( 
    COMMAND $ENV{environment}/python/x64/python.exe ${CMAKE_CURRENT_BINARY_DIR}/gen_resources.py
  )
endif()

# Configure Qt
find_package(Qt5Qml REQUIRED)
find_package(Qt5Quick REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5LinguistTools)

# Windows application icon
if (WIN32)
  set(WINDOWS_RES_FILE ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.obj)
  if (MSVC)
    add_custom_command(OUTPUT ${WINDOWS_RES_FILE}
      COMMAND rc.exe /fo ${WINDOWS_RES_FILE} ${PROJECT_BINARY_DIR}/module.rc
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/win
    )
  else()
    add_custom_command(OUTPUT ${WINDOWS_RES_FILE}
      COMMAND windres.exe ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME} ${WINDOWS_RES_FILE}
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/win
    )
  endif()
endif()

QT5_CREATE_TRANSLATION(...)

if (NOT MSVC)
  # Enable the C++11 standard
  set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -std=c++11)
endif()

file(GLOB_RECURSE UI_FILES src/*.ui)
file(GLOB_RECURSE CODE_FILES src/*.cpp)

qt5_wrap_ui(UI_HEADERS ${UI_FILES})
qt5_add_resources(RESOURCE_FILES ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc)

add_executable(${PROJECT_SHORTNAME} WIN32
  ${UI_HEADERS}
  ${CODE_FILES}
  ${RESOURCE_FILES}
  ${WINDOWS_RES_FILE}
)
target_link_libraries(${PROJECT_SHORTNAME}
  Qt5::Qml Qt5::Quick Qt5::Core Qt5::Gui Qt5::Network Ws2_32.lib glu32.lib opengl32.lib odbccp32.lib
)
set_target_properties(${PROJECT_SHORTNAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "src/stdafx.h")
cotire(${PROJECT_SHORTNAME})

# Testing configuration
# enable_testing()
# set(TEST_LINK_LIBRARIES Qt5::Test)

if (UNIX)
  install(TARGETS ${PROJECT_SHORTNAME}
          RUNTIME DESTINATION bin)
elseif (WIN32)
  install(TARGETS ${PROJECT_SHORTNAME}
          DESTINATION .)
endif()

#if (WIN32)
#  add_subdirectory(win)
#endif()
