set(library_type)
#if(WINDOWS)
#    set(library_type LIBRARY_TYPE SHARED)
#endif()

nx_add_target(nx_sdk LIBRARY ${library_type} NO_MOC)

target_compile_definitions(nx_sdk
    PRIVATE NX_SDK_API=
    INTERFACE NX_SDK_API=
)
