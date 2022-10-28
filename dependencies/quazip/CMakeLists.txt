## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(quazip_dir ${open_source_root}/artifacts/quazip/quazip)

file(GLOB sources CONFIGURE_DEPENDS "${quazip_dir}/*.c" "${quazip_dir}/*.cpp")
file(GLOB headers CONFIGURE_DEPENDS "${quazip_dir}/*.h")
qt5_wrap_cpp(moc_sources
    ${quazip_dir}/quazipfile.h
    ${quazip_dir}/quagzipfile.h
    ${quazip_dir}/quaziodevice.h
)

add_definitions(-DQUAZIP_BUILD)

if(IOS)
    set(quazip_library_type "STATIC")
else()
    set(quazip_library_type "SHARED")
endif()

add_library(quazip ${quazip_library_type} ${sources} ${moc_sources})

set_target_properties(quazip PROPERTIES FOLDER third_party)

target_include_directories(quazip PUBLIC ${quazip_dir}/..)
if(WINDOWS)
    target_include_directories(quazip PUBLIC ${QT_DIR}/include/QtZlib)
endif()

target_link_libraries(quazip Qt5::Core)

if(NOT WINDOWS)
    target_link_libraries(quazip z)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(quazip PRIVATE -Wno-deprecated-declarations -Wno-misleading-indentation)
endif()
