find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})
generate_qrc(${CMAKE_CURRENT_BINARY_DIR}/appserver2.qrc
    "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")

add_library(appserver2
    ${CPP_FILES} ${MOC_FILES}
    src/StdAfx.cpp src/StdAfx.h
    ${CMAKE_CURRENT_BINARY_DIR}/appserver2.qrc
)

add_precompiled_header(appserver2 src/StdAfx.h FORCEINCLUDE)

target_include_directories(appserver2 PUBLIC src)

target_link_libraries(appserver2 PUBLIC nx_network)
