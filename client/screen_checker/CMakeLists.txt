add_definitions(-DUNICODE -D_UNICODE)
add_compile_options(/MT)

nx_add_target(screen_checker EXECUTABLE
    NO_PCH
    PRIVATE_LIBS
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
)
