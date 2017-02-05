set(product.title "${company.name} libvms_gateway")
set(product.display.title "${company.name} VMS Gateway")

find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

generate_qrc(${CMAKE_CURRENT_BINARY_DIR}/libvms_gateway.qrc
    "${CMAKE_CURRENT_SOURCE_DIR}/static-resources")

nx_configure_file(
    maven/filter-resources/libvms_gateway_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

add_library(libvms_gateway
    ${CPP_FILES} ${MOC_FILES}
    ${CMAKE_CURRENT_BINARY_DIR}/libvms_gateway_app_info_impl.cpp
    src/StdAfx.cpp src/StdAfx.h
    ${CMAKE_CURRENT_BINARY_DIR}/libvms_gateway.qrc)

add_precompiled_header(libvms_gateway src/StdAfx.h FORCEINCLUDE)

target_include_directories(libvms_gateway PUBLIC src)

target_link_libraries(libvms_gateway PUBLIC cloud_db_client nx_network)
