set(src_dir ${CMAKE_SOURCE_DIR}/artifacts/qt-solutions/qtsingleapplication)

add_library(qtsingleapplication STATIC
    ${src_dir}/src/qtsingleapplication.cpp
    ${src_dir}/src/qtlocalpeer.cpp
)
nx_add_qt_mocables(qtsingleapplication
    ${src_dir}/src/qtsingleapplication.h
    ${src_dir}/src/qtlocalpeer.h
)

target_include_directories(qtsingleapplication PUBLIC ${src_dir}/src)
target_link_libraries(qtsingleapplication PRIVATE Qt5::Network Qt5::Widgets)
