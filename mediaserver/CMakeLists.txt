find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

generate_qrc("${CMAKE_CURRENT_BINARY_DIR}/mediaserver.qrc"
    "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")

add_executable(mediaserver
    ${CPP_FILES} ${MOC_FILES}
    src/StdAfx.cpp src/StdAfx.h
    "${CMAKE_CURRENT_BINARY_DIR}/mediaserver.qrc")

add_precompiled_header(mediaserver src/StdAfx.h FORCEINCLUDE)

target_include_directories(mediaserver PRIVATE src)

target_link_libraries(mediaserver mediaserver_core festival)
