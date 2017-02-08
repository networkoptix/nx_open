find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

add_executable(desktop_client
    ${CPP_FILES} ${MOC_FILES}
    src/StdAfx.cpp src/StdAfx.h)

add_precompiled_header(desktop_client src/StdAfx.h FORCEINCLUDE)

rdep_add_package(any/festival-vox)
rdep_add_package(any/help)

target_include_directories(desktop_client PRIVATE src)

target_link_libraries(desktop_client
    PRIVATE
        client qtsingleapplication festival)
