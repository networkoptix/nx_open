set(quazip_dir ${CMAKE_SOURCE_DIR}/artifacts/quazip/quazip)

file(GLOB sources "${quazip_dir}/*.c" "${quazip_dir}/*.cpp")
file(GLOB headers "${quazip_dir}/*.h")
qt5_wrap_cpp(moc_sources ${headers})

add_definitions(-DQUAZIP_BUILD)

add_library(quazip SHARED ${sources} ${moc_sources})

target_include_directories(quazip PUBLIC ${quazip_dir}/..)
if(WINDOWS)
    target_include_directories(quazip PUBLIC ${QT_DIR}/include/QtZlib)
endif()

target_link_libraries(quazip Qt5::Core)
if(NOT WINDOWS)
    target_link_libraries(quazip z)
endif()
