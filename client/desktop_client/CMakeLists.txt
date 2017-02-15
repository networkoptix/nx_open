rdep_add_package(any/help)

nx_add_target(desktop_client EXECUTABLE
    PUBLIC_LIBS qtsingleapplication libclient)

if(LINUX)
    target_link_libraries(desktop_client PUBLIC festival)
endif()
