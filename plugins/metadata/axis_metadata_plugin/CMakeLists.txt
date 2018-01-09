#nx_add_target(axis_metadata_plugin LIBRARY
#    NO_MOC NO_PCH
#        PUBLIC_LIBS
#            common)

nx_add_target(axis_metadata_plugin LIBRARY
    NO_PCH
    PRIVATE_LIBS
        Qt5::Core Qt5::Network Qt5::Sql
        #ffmpeg openssl
        common nx_network nx_utils nx_fusion 
	mediaserver_core 
	nx_kit
    )


# Required for GSOAP core and generated files.
include_directories(
    src/gsoap/import
    src/gsoap/aux_src
)

