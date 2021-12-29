## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(src_dir ${open_source_root}/artifacts/qt-solutions/qtsingleapplication)

add_library(qtsinglecoreapplication STATIC
    ${src_dir}/src/qtsinglecoreapplication.cpp
    ${src_dir}/src/qtlocalpeer.cpp
)
set_target_properties(qtsinglecoreapplication PROPERTIES FOLDER third_party)

nx_add_qt_mocables(qtsinglecoreapplication
    ${src_dir}/src/qtsinglecoreapplication.h
    ${src_dir}/src/qtlocalpeer.h
)

target_include_directories(qtsinglecoreapplication PUBLIC ${src_dir}/src)
target_link_libraries(qtsinglecoreapplication PRIVATE Qt6::Network)
