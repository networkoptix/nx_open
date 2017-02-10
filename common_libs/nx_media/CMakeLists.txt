nx_add_target(nx_media LIBRARY PUBLIC_LIBS nx_audio)

target_include_directories(nx_media PRIVATE ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS})

