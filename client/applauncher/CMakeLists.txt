set(product.title "${applauncher.name}")

find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

nx_configure_file(maven/filter-resources/applauncher_app_info_impl.cpp
    ${CMAKE_CURRENT_BINARY_DIR})

add_executable(applauncher
    ${CPP_FILES} ${MOC_FILES}
    src/StdAfx.cpp src/StdAfx.h
    ${CMAKE_CURRENT_BINARY_DIR}/applauncher_app_info_impl.cpp)

add_precompiled_header(applauncher src/StdAfx.h FORCEINCLUDE)

target_include_directories(applauncher PRIVATE src)

target_link_libraries(applauncher qtsinglecoreapplication nx_network)
