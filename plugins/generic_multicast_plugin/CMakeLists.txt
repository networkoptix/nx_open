nx_add_target(generic_multicast_plugin LIBRARY
    NO_MOC NO_PCH 
    PUBLIC_LIBS 
        Qt5::Core Qt5::Network Qt5::Multimedia
        ffmpeg
        common nx_network nx_utils)
