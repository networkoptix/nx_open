project(nx_utils)
set(PROJECT_SHORTNAME "nx_utils")
set(PROJECT_LONGNAME "${nxtool.company.name} Utilities Library")

remove_definitions(-D_CONSOLE
                -DNX_UTILS_API=__declspec\(dllimport\)
                )
add_definitions(-D_CONSOLE
                -DNX_UTILS_API=__declspec\(dllexport\)
                )

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
  Qt5::Core DbgHelp.lib Iphlpapi.lib Ole32.lib Secur32.lib winmm.lib
)
#set_target_properties(${PROJECT_SHORTNAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "src/stdafx.h")
#cotire(${PROJECT_SHORTNAME})
