find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

generate_qrc(${CMAKE_CURRENT_BINARY_DIR}/libclient_core.qrc
    "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")

add_library(client_core
    ${CPP_FILES} ${MOC_FILES}
    ${CMAKE_CURRENT_BINARY_DIR}/libclient_core.qrc
    src/StdAfx.cpp src/StdAfx.h)

add_precompiled_header(client_core src/StdAfx.h FORCEINCLUDE)

target_include_directories(client_core
    PUBLIC
        ${Qt5Quick_PRIVATE_INCLUDE_DIRS}
        ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS}
        "${QT_DIR}/include/QtMultimediaQuick_p"
        "${QT_DIR}/include/QtMultimediaQuick_p/${qt_version}"
        "${QT_DIR}/include/QtMultimediaQuick_p/${qt_version}/QtMultimediaQuick_p"
        src)

find_library(Qt5MultimediaQuickTools_LIBS NAMES Qt5MultimediaQuick_p HINTS "${QT_DIR}/lib")

target_link_libraries(client_core
    PUBLIC
        Qt5::Quick
        ${Qt5MultimediaQuickTools_LIBS}
        appserver2 nx_vms_utils cloud_db_client nx_media)
