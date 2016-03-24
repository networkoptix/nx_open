project(nx_email)
set(PROJECT_SHORTNAME "nx_email")
set(PROJECT_LONGNAME "${nxtool.company.name} E-Mail Library")

find_sources(src)
ADD_MSVC_PRECOMPILED_HEADER("StdAfx.h" "${PROJECT_SOURCE_DIR}/src/StdAfx.cpp" CPP_FILES)
process_resources()
add_translations(translations)
qt5_wrap_ui(UI_HEADERS ${UI_FILES})
qt5_add_resources(RESOURCE_FILES ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc)

find_package(Qt5Network REQUIRED)
find_package(Qt5Concurrent REQUIRED)

add_library(${PROJECT_SHORTNAME} ${DEFAULT_LIBRARY_TYPE}
  ${UI_HEADERS}
  ${CPP_FILES}
  ${RESOURCE_FILES}
)
target_link_libraries(${PROJECT_SHORTNAME}
  Qt5::Core Qt5::Network Qt5::Concurrent
)
#set_target_properties(${PROJECT_SHORTNAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "src/stdafx.h")
#cotire(${PROJECT_SHORTNAME})
