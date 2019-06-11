nx_find_files(ui_files src "ui")
qt5_wrap_ui(uic_files ${ui_files})

nx_add_target(nov_browser EXECUTABLE
    ADDITIONAL_SOURCES
        ${uic_files}
    RC_FILE
        "nov_browser.rc"
    PUBLIC_LIBS
        nx_vms_utils nx_utils nx_vms_client_core
    PRIVATE_LIBS
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
)


