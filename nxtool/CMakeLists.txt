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
              
project(nxtool)
set(PROJECT_SHORTNAME "nxtool")
set(PROJECT_LONGNAME "${nxtool.company.name} Server Tool")
set(PROJECT_VERSION "2.5.0")

include(default_project_properties)

# Additional Include Directories
include_directories (${include_directories} src)

# Configure Qt
find_package(Qt5Qml REQUIRED)
find_package(Qt5Quick REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)


#precompiled_header()
find_sources(src)
#process_rc()
#todo: not working as function. Rewrite.
if (WIN32)
  set(WINDOWS_RES_FILE ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.obj)
  if (MSVC)
    add_custom_command(OUTPUT ${WINDOWS_RES_FILE}
      COMMAND rc.exe /fo ${WINDOWS_RES_FILE} ${PROJECT_BINARY_DIR}/module.rc
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )
    message ("Processing Resources from ${PROJECT_BINARY_DIR}/module.rc")
  else()
    add_custom_command(OUTPUT ${WINDOWS_RES_FILE}
      COMMAND windres.exe ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME} ${WINDOWS_RES_FILE}
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )
  endif()
endif()
find_sources(src)
process_resources()
add_translations(translations)
qt5_wrap_ui(UI_HEADERS ${UI_FILES})
qt5_add_resources(RESOURCE_FILES ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc)

add_executable(${PROJECT_SHORTNAME} WIN32
  ${UI_HEADERS}
  ${CPP_FILES}
  ${RESOURCE_FILES}
  ${WINDOWS_RES_FILE}
)
target_link_libraries(${PROJECT_SHORTNAME}
  Qt5::Qml Qt5::Quick Qt5::Core Qt5::Gui Qt5::Network Ws2_32.lib glu32.lib opengl32.lib odbccp32.lib
)
set_target_properties(${PROJECT_SHORTNAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "src/stdafx.h")
cotire(${PROJECT_SHORTNAME})

if (UNIX)
  install(TARGETS ${PROJECT_SHORTNAME}
          RUNTIME DESTINATION bin)
elseif (WIN32)
  install(TARGETS ${PROJECT_SHORTNAME}
          DESTINATION .)
endif()