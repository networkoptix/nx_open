find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

add_executable(cloud_db
    ${CPP_FILES} ${MOC_FILES}
    src/StdAfx.cpp src/StdAfx.h)

add_precompiled_header(cloud_db src/StdAfx.h FORCEINCLUDE)

target_include_directories(cloud_db PRIVATE src)

target_link_libraries(cloud_db libcloud_db)
