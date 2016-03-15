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
 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
 
project(appserver2)
set(PROJECT_SHORTNAME "appserver2")
set(PROJECT_LONGNAME "${nxtool.company.name} New Enterprise Controller")

# Additional Include Directories
include_directories (${include_directories} src)        

find_sources(src)
ADD_MSVC_PRECOMPILED_HEADER("StdAfx.h" "${PROJECT_SOURCE_DIR}/src/StdAfx.cpp" CPP_FILES)
process_resources()
add_translations(translations)
qt5_wrap_ui(UI_HEADERS ${UI_FILES})
qt5_add_resources(RESOURCE_FILES ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc)

find_package(Qt5Network REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5Sql REQUIRED)
find_package(Qt5Concurrent REQUIRED)

add_library(${PROJECT_SHORTNAME} STATIC
  ${UI_HEADERS}
  ${CPP_FILES}
  ${RESOURCE_FILES}
)
target_link_libraries(${PROJECT_SHORTNAME}
  Qt5::Core common nx_streaming Qt5::Network Qt5::Sql Qt5::Concurrent Ws2_32.lib DbgHelp.lib winmm.lib
)
