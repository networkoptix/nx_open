set(src_dir ${CMAKE_SOURCE_DIR}/artifacts/qt-solutions/qtsingleapplication)

add_library(qtsinglecoreapplication STATIC
    ${src_dir}/src/qtsinglecoreapplication.cpp
    ${src_dir}/src/qtlocalpeer.cpp
)
nx_add_qt_mocables(qtsinglecoreapplication
    ${src_dir}/src/qtsinglecoreapplication.h
    ${src_dir}/src/qtlocalpeer.h
)

target_include_directories(qtsinglecoreapplication PUBLIC ${src_dir}/src)
target_link_libraries(qtsinglecoreapplication PRIVATE Qt5::Network)
