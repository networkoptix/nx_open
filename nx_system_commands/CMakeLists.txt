if (WINDOWS)
    set(nx_system_commands_private_libs common Qt5::Core)
endif()

nx_add_target(nx_system_commands LIBRARY LIBRARY_TYPE STATIC
    PUBLIC_LIBS
        boost
    PRIVATE_LIBS
        ${nx_system_commands_private_libs}) 

