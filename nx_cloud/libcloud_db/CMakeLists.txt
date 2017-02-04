find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

generate_qrc(${CMAKE_CURRENT_BINARY_DIR}/libcloud_db.qrc
    "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")

nx_configure_file(
    maven/filter-resources/libcloud_db_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

add_library(libcloud_db
    ${CPP_FILES} ${MOC_FILES}
    ${CMAKE_CURRENT_BINARY_DIR}/libcloud_db_app_info_impl.cpp
    src/StdAfx.cpp src/StdAfx.h
    ${CMAKE_CURRENT_BINARY_DIR}/libcloud_db.qrc)

set_target_properties(libcloud_db PROPERTIES PREFIX "")

add_precompiled_header(libcloud_db src/StdAfx.h FORCEINCLUDE)

target_include_directories(libcloud_db
    PUBLIC
        ${PROJECT_SOURCE_DIR}/nx_cloud
        src)

target_link_libraries(libcloud_db
    PUBLIC cloud_db_client appserver2 nx_email)
