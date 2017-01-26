project(udt)
set(PROJECT_SHORTNAME "udt")
set(PROJECT_LONGNAME "${short.company.name} UDP-based Data Transfer Library")

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
