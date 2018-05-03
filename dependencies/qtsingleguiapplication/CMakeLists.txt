set(src_dir ${CMAKE_SOURCE_DIR}/artifacts/qt-solutions/qtsingleapplication)

add_library(qtsingleguiapplication STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qtsingleguiapplication.cpp
    ${src_dir}/src/qtlocalpeer.cpp
)
nx_add_qt_mocables(qtsingleguiapplication
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qtsingleguiapplication.h
    ${src_dir}/src/qtlocalpeer.h
)

target_include_directories(qtsingleguiapplication
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${src_dir}/src
)
target_link_libraries(qtsingleguiapplication PRIVATE Qt5::Network Qt5::Gui)
