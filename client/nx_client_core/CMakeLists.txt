find_library(Qt5MultimediaQuick_p_LIBRARY
    NAMES Qt5MultimediaQuick_p QtMultimediaQuick_p
    PATHS "${QT_DIR}/lib" NO_DEFAULT_PATH)
mark_as_advanced(Qt5MultimediaQuick_p_LIBRARY)

nx_add_target(nx_client_core LIBRARY
    PUBLIC_LIBS
        Qt5::Quick
        ${Qt5MultimediaQuick_p_LIBRARY}
        appserver2 nx_vms_utils cloud_db_client nx_media)

set_target_properties(nx_client_core PROPERTIES PREFIX "")

target_include_directories(nx_client_core
    PUBLIC
        ${Qt5Quick_PRIVATE_INCLUDE_DIRS}
        ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS})
