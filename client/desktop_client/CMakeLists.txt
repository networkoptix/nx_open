nx_add_target(desktop_client EXECUTABLE
    PUBLIC_LIBS qtsingleapplication nx_kit nx_client_desktop)

project(desktop_client)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT desktop_client)
