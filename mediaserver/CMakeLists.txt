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

rdep_add_package(any/server-external-${branch} OPTIONAL PATH_VARIABLE server_external_path)
if(NOT server_external_path)
    rdep_add_package(any/server-external-${releaseVersion})
endif()
