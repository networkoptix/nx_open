nx_rdep_add_package(any/help)

nx_add_target(desktop_client EXECUTABLE
    PUBLIC_LIBS qtsingleapplication nx_kit nx_client_desktop)
