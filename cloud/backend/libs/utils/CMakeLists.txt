nx_add_target(nx_cloud_utils LIBRARY NO_MOC
    WERROR_IF NOT WINDOWS
    PUBLIC_LIBS nx_network)

set_target_properties(nx_cloud_utils PROPERTIES LINKER_LANGUAGE CXX)