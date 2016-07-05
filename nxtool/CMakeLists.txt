project(nxtool)
set(PROJECT_SHORTNAME "nxtool")
set(PROJECT_LONGNAME "${short.company.name} Server Tool")
if(WIN32)
    set(WINDOWS_RES_FILE ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.obj)
endif()
include(default_project_properties)

# Additional Include Directories
include_directories (${include_directories} src)

# Configure Qt
find_package(Qt5Qml REQUIRED)
find_package(Qt5Quick REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)


find_sources(src)
ADD_MSVC_PRECOMPILED_HEADER("StdAfx.h" "${PROJECT_SOURCE_DIR}/src/StdAfx.cpp" CPP_FILES)
process_rc()
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
#set_target_properties(${PROJECT_SHORTNAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "src/stdafx.h")
#cotire(${PROJECT_SHORTNAME})

if (UNIX)
  install(TARGETS ${PROJECT_SHORTNAME}
          RUNTIME DESTINATION bin)
elseif (WIN32)
  install(TARGETS ${PROJECT_SHORTNAME}
          DESTINATION .)
endif()
