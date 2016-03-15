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

project(udt)
set(PROJECT_SHORTNAME "udt")
set(PROJECT_LONGNAME "${nxtool.company.name} UDP-based Data Transfer Library")

remove_definitions(-D_CONSOLE
                -DUDT_API=__declspec\(dllimport\)
                )
add_definitions(-D_CONSOLE
                -DUDT_API=__declspec\(dllexport\)
                )
#include_directories (${include_directories} src $ENV{environment}/include)
                
find_sources(src)
ADD_MSVC_PRECOMPILED_HEADER("StdAfx.h" "${PROJECT_SOURCE_DIR}/src/StdAfx.cpp" CPP_FILES)
process_resources()
add_translations(translations)
qt5_wrap_ui(UI_HEADERS ${UI_FILES})
qt5_add_resources(RESOURCE_FILES ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc)

add_library(${PROJECT_SHORTNAME} SHARED
  ${UI_HEADERS}
  ${CPP_FILES}
  ${RESOURCE_FILES}
)
target_link_libraries(${PROJECT_SHORTNAME}
  Ws2_32.lib
)
