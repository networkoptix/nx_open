project(nx_audio)
set(PROJECT_SHORTNAME "nx_audio")
set(PROJECT_LONGNAME "${nxtool.company.name} Audio Library")

find_sources(src)
ADD_MSVC_PRECOMPILED_HEADER("StdAfx.h" "${PROJECT_SOURCE_DIR}/src/StdAfx.cpp" CPP_FILES)
process_resources()
add_translations(translations)
qt5_wrap_ui(UI_HEADERS ${UI_FILES})
qt5_add_resources(RESOURCE_FILES ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc)

add_library(${PROJECT_SHORTNAME} ${DEFAULT_LIBRARY_TYPE}
  ${UI_HEADERS}
  ${CPP_FILES}
  ${RESOURCE_FILES}
)
target_link_libraries(${PROJECT_SHORTNAME}
  Qt5::Core
)
