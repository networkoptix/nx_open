set(src_dir ${CMAKE_SOURCE_DIR}/artifacts/qt-solutions/qtservice)
set(cpp_files ${src_dir}/src/qtservice.cpp)
set(mocables ${src_dir}/src/qtservice.cpp)

if(WIN32)
    list(APPEND cpp_files
        ${src_dir}/src/qtservice_win.cpp
    )
    list(APPEND mocables
        ${src_dir}/src/qtservice_win.cpp
    )
elseif(UNIX)
    list(APPEND cpp_files
        ${src_dir}/src/qtservice_unix.cpp
        ${src_dir}/src/qtunixsocket.cpp
        ${src_dir}/src/qtunixserversocket.cpp
    )
    list(APPEND mocables
        ${src_dir}/src/qtunixsocket.h
        ${src_dir}/src/qtunixserversocket.h
        ${src_dir}/src/qtservice_unix.cpp
    )
endif()

add_library(qtservice STATIC ${cpp_files})
nx_add_qt_mocables(qtservice ${mocables})

target_include_directories(qtservice PUBLIC ${src_dir}/src)
target_link_libraries(qtservice PRIVATE Qt5::Network)
