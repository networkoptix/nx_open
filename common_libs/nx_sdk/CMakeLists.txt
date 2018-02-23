set(library_type)
#if(WINDOWS)
#    set(library_type LIBRARY_TYPE SHARED)
#endif()

nx_add_target(nx_sdk LIBRARY ${library_type} NO_MOC NO_PCH
    PRIVATE_LIBS
        nx_kit
)
