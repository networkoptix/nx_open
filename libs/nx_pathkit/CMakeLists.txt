## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(Qt6 COMPONENTS ShaderTools)
find_package(pathkit)

nx_add_target(nx_pathkit LIBRARY NO_MOC
    PUBLIC_LIBS
        nx_utils
        Qt6::Gui
        pathkit::pathkit
    FOLDER common/libs
)

qt6_add_shaders(nx_pathkit "pathkit_shaders"
    PREFIX
        "/nx_pathkit"
    FILES
        "shaders/color.frag"
        "shaders/color.vert"
        "shaders/texture.frag"
        "shaders/texture.vert"
)

target_compile_definitions(nx_pathkit
    PRIVATE NX_PATHKIT_API=${API_EXPORT_MACRO}
    INTERFACE NX_PATHKIT_API=${API_IMPORT_MACRO})

target_include_directories(nx_pathkit INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(nx_pathkit
    PRIVATE
        ${Qt6Gui_PRIVATE_INCLUDE_DIRS})

if(withTests)
    add_subdirectory(unit_tests)
endif()
