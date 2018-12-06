add_definitions(-DUNICODE -D_UNICODE)
add_compile_options(/MT)

nx_configure_file(version.h ${CMAKE_CURRENT_BINARY_DIR})

nx_add_target(nov_file_launcher EXECUTABLE SIGNED
    NO_MOC
    ADDITIONAL_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/src/launcher.rc"
    PRIVATE_LIBS
        msi
        comctl32
        boost
)

set_target_properties(nov_file_launcher PROPERTIES
    OUTPUT_NAME "launcher"
    WIN32_EXECUTABLE ON
)
