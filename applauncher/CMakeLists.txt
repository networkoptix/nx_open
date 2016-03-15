cmake_minimum_required(VERSION 3.0)

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
              
project(applauncher)
set(PROJECT_SHORTNAME "applauncher")
set(PROJECT_LONGNAME "${nxtool.company.name} Application Launcher")
set(WINDOWS_RES_FILE ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.obj)
include(default_project_properties)

# Additional Include Directories
include_directories (${include_directories} src)

# Configure Qt
find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Xml REQUIRED)

find_sources(src)
add_qtsinglecoreapplication()
ADD_MSVC_PRECOMPILED_HEADER("StdAfx.h" "${PROJECT_SOURCE_DIR}/src/StdAfx.cpp" CPP_FILES)
process_rc()
process_resources()
add_translations(translations)
qt5_wrap_ui(UI_HEADERS ${UI_FILES})
qt5_add_resources(RESOURCE_FILES ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc)

foreach(filename ${CPP_FILES})
  message("Adding source: " ${filename})
endforeach(filename ${CPP_FILES})     

add_executable(${PROJECT_SHORTNAME} WIN32
  ${UI_HEADERS}
  ${CPP_FILES}
  ${RESOURCE_FILES}
  ${WINDOWS_RES_FILE}
)
target_link_libraries(${PROJECT_SHORTNAME}
  Qt5::Core Qt5::Gui Qt5::Network Qt5::Xml Qt5::Widgets quazip common nx_utils nx_network nx_streaming udt DbgHelp.lib Ws2_32.lib Iphlpapi.lib Winmm.lib
)
#set_target_properties(${PROJECT_SHORTNAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "src/stdafx.h")
#cotire(${PROJECT_SHORTNAME})

if (UNIX)
  install(TARGETS ${PROJECT_SHORTNAME}
          RUNTIME DESTINATION bin)
elseif (WIN32)
  install(TARGETS ${PROJECT_SHORTNAME}
          DESTINATION .)
endif()
