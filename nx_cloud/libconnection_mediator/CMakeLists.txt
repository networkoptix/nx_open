set(product.title "${company.name} Mediator Lib")
set(product.display.title "${company.name} Mediator Lib")

find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

nx_configure_file(
    maven/filter-resources/libconnection_mediator_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

add_library(libconnection_mediator
    ${CPP_FILES} ${MOC_FILES}
    ${CMAKE_CURRENT_BINARY_DIR}/libconnection_mediator_app_info_impl.cpp
    src/StdAfx.cpp src/StdAfx.h)

set_target_properties(libconnection_mediator PROPERTIES PREFIX "")

add_precompiled_header(libconnection_mediator src/StdAfx.h FORCEINCLUDE)

target_include_directories(libconnection_mediator PUBLIC src)

target_link_libraries(libconnection_mediator
    PRIVATE
        qtservice qtsinglecoreapplication
    PUBLIC
        cloud_db_client nx_network)
