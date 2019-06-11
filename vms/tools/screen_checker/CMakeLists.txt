add_compile_options(/MT)

nx_add_target(screen_checker EXECUTABLE
    PRIVATE_LIBS
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
)
