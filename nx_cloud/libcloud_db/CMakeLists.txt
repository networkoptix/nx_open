find_sources(src CPP_FILES)
generate_qrc(${CMAKE_CURRENT_BINARY_DIR}/cloud_db.qrc
    "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")

configure_file(
    maven/filter-resources/libcloud_db_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/libcloud_db_app_info_impl.cpp
)

add_library(cloud_db
    ${CPP_FILES}
    ${CMAKE_CURRENT_BINARY_DIR}/libcloud_db_app_info_impl.cpp
    src/StdAfx.cpp src/StdAfx.h
    ${CMAKE_CURRENT_BINARY_DIR}/cloud_db.qrc
)

add_precompiled_header(cloud_db src/StdAfx.h FORCEINCLUDE)

target_include_directories(cloud_db
    PUBLIC
        ${PROJECT_SOURCE_DIR}/nx_cloud
        src
)

target_link_libraries(cloud_db
    PUBLIC cloud_db_client appserver2 nx_email)
