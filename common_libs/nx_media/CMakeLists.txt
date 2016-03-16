project(nx_media)
set(PROJECT_SHORTNAME "nx_media")
set(PROJECT_LONGNAME "${nxtool.company.name} Multimedia Library")

find_sources(src)
ADD_MSVC_PRECOMPILED_HEADER("StdAfx.h" "${PROJECT_SOURCE_DIR}/src/StdAfx.cpp" CPP_FILES)
process_resources()
add_translations(translations)
qt5_wrap_ui(UI_HEADERS ${UI_FILES})
qt5_add_resources(RESOURCE_FILES ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc)

find_package(Qt5Gui REQUIRED)
find_package(Qt5OpenGL REQUIRED)
find_package(Qt5Multimedia REQUIRED)

add_library(${PROJECT_SHORTNAME} ${DEFAULT_LIBRARY_TYPE}
  ${UI_HEADERS}
  ${CPP_FILES}
  ${RESOURCE_FILES}
)
target_link_libraries(${PROJECT_SHORTNAME}
  Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Multimedia common
)
