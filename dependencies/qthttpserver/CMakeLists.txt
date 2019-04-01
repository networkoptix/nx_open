set(qthttpserver_dir ${CMAKE_SOURCE_DIR}/artifacts/qthttpserver)

file(GLOB sources "${qthttpserver_dir}/*.cpp")
file(GLOB headers "${qthttpserver_dir}/*.h")
qt5_wrap_cpp(moc_sources ${headers})

add_library(qthttpserver
    ${headers} ${sources} ${moc_sources}
)

target_link_libraries(qthttpserver
    PUBLIC
        Qt5::Core Qt5::Network
)

target_include_directories(qthttpserver
    PUBLIC
        ${qthttpserver_dir}
)
