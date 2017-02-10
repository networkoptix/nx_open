find_library(Qt5MultimediaQuickTools_LIBS NAMES Qt5MultimediaQuick_p
    PATHS "${QT_DIR}/lib" NO_DEFAULT_PATH)

nx_add_target(libclient_core LIBRARY
    PUBLIC_LIBS
        Qt5::Quick
        ${Qt5MultimediaQuickTools_LIBS}
        appserver2 nx_vms_utils cloud_db_client nx_media)

set_target_properties(libclient_core PROPERTIES PREFIX "")

target_include_directories(libclient_core
    PUBLIC
        ${Qt5Quick_PRIVATE_INCLUDE_DIRS}
        ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS}
        "${QT_DIR}/include/QtMultimediaQuick_p"
        "${QT_DIR}/include/QtMultimediaQuick_p/${qt_version}"
        "${QT_DIR}/include/QtMultimediaQuick_p/${qt_version}/QtMultimediaQuick_p")
