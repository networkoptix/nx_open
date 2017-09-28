find_library(Qt5MultimediaQuick_p_LIBRARY_OPTIMIZED
    NAMES Qt5MultimediaQuick_p QtMultimediaQuick_p
    PATHS "${QT_DIR}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
find_library(Qt5MultimediaQuick_p_LIBRARY_DEBUG
    NAMES Qt5MultimediaQuick_pd QtMultimediaQuick_pd Qt5MultimediaQuick_p QtMultimediaQuick_p
    PATHS "${QT_DIR}/lib" NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)

if(NOT Qt5MultimediaQuick_p_LIBRARY_OPTIMIZED OR NOT Qt5MultimediaQuick_p_LIBRARY_DEBUG)
    message(FATAL_ERROR "Library Qt5MultimediaQuick_p not found.")
endif()

mark_as_advanced(Qt5MultimediaQuick_p_LIBRARY_OPTIMIZED Qt5MultimediaQuick_p_LIBRARY_DEBUG)

nx_add_target(nx_client_core LIBRARY
    PUBLIC_LIBS
        Qt5::Quick
        appserver2 nx_vms_utils cloud_db_client nx_media nx_network nx_utils)

target_link_libraries(nx_client_core PUBLIC
    debug ${Qt5MultimediaQuick_p_LIBRARY_DEBUG}
    optimized ${Qt5MultimediaQuick_p_LIBRARY_OPTIMIZED})

target_include_directories(nx_client_core
    PUBLIC
        ${Qt5Quick_PRIVATE_INCLUDE_DIRS}
        ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS})
