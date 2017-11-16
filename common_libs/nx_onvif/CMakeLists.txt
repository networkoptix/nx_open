find_files(src "c" c_files)
nx_add_target(nx_onvif LIBRARY LIBRARY_TYPE STATIC NO_MOC NO_PCH
    ADDITIONAL_SOURCES ${c_files}
    PUBLIC_LIBS openssl
)

set_source_files_properties(${c_files} PROPERTIES LANGUAGE CXX)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/onvif)
add_definitions(-DWITH_OPENSSL -DWITH_PURE_VIRTUAL)

target_include_directories(nx_onvif PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/gsoap)

if(NOT WINDOWS)
    target_link_libraries(nx_onvif PUBLIC pthread)
endif()
