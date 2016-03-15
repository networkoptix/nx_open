project(nx_network)
set(PROJECT_SHORTNAME "nx_network")
set(PROJECT_LONGNAME "${nxtool.company.name} Network Library")

remove_definitions(-D_CONSOLE
                -DNX_NETWORK_API=__declspec\(dllimport\)
                )
add_definitions(-D_CONSOLE
                -DNX_NETWORK_API=__declspec\(dllexport\)
                )

find_sources(src)
ADD_MSVC_PRECOMPILED_HEADER("StdAfx.h" "${PROJECT_SOURCE_DIR}/src/StdAfx.cpp" CPP_FILES)
process_resources()
add_translations(translations)
qt5_wrap_ui(UI_HEADERS ${UI_FILES})
qt5_add_resources(RESOURCE_FILES ${PROJECT_BINARY_DIR}/${PROJECT_SHORTNAME}.qrc)

find_package(Qt5Gui REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5XmlPatterns REQUIRED)
find_package(Qt5Sql REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5Multimedia REQUIRED)

add_library(${PROJECT_SHORTNAME} SHARED
  ${UI_HEADERS}
  ${CPP_FILES}
  ${RESOURCE_FILES}
)
target_link_libraries(${PROJECT_SHORTNAME}
  common nx_utils nx_email udt Qt5::Core libeay32.lib ssleay32.lib Qt5::Gui Qt5::Network Ws2_32.lib DbgHelp.lib sigar.lib odbccp32.lib
)
