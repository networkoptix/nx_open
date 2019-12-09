set(nx_system_commands_private_libs Qt5::Core nx_kit)

if (LINUX)
    add_definitions(-DNX_CUSTOMIZATION_NAME=${customization.companyId})
endif()

nx_add_target(nx_system_commands LIBRARY LIBRARY_TYPE STATIC
    PRIVATE_LIBS
        ${nx_system_commands_private_libs})

