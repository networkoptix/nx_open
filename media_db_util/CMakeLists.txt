find_package(Qt5 COMPONENTS Core)

nx_add_target(media_db_util EXECUTABLE
    PUBLIC_LIBS mediaserver_core Qt5::Core)

