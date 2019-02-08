set(nx_system_commands_private_libs Qt5::Core)

if (LINUX)
    add_definitions(-DNX_CUSTOMIZATION_NAME=${deb.customization.company.name})
endif()

nx_add_target(nx_system_commands LIBRARY LIBRARY_TYPE STATIC
    PUBLIC_LIBS
        boost
    PRIVATE_LIBS
        ${nx_system_commands_private_libs})

