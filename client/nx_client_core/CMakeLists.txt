if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(Qt5MultimediaQuick_names
        Qt5MultimediaQuickd QtMultimediaQuickd Qt5MultimediaQuick QtMultimediaQuick)
else()
    set(Qt5MultimediaQuick_names Qt5MultimediaQuick QtMultimediaQuick)
endif()

find_library(Qt5MultimediaQuick_LIBRARY
    NAMES ${Qt5MultimediaQuick_names}
    PATHS "${QT_DIR}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)

if(NOT Qt5MultimediaQuick_LIBRARY)
    message(FATAL_ERROR "Library Qt5MultimediaQuick not found.")
endif()

mark_as_advanced(Qt5MultimediaQuick_LIBRARY)

nx_add_target(nx_client_core LIBRARY
    PUBLIC_LIBS
        Qt5::Quick ${Qt5MultimediaQuick_LIBRARY}
        qtkeychain
        appserver2 nx_vms_utils cloud_db_client nx_media nx_network nx_utils nx_update)

target_include_directories(nx_client_core
    PUBLIC
        ${Qt5Quick_PRIVATE_INCLUDE_DIRS}
        ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS})
