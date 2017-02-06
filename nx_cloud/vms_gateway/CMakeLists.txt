find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

add_executable(vms_gateway
    ${CPP_FILES} ${MOC_FILES}
    src/StdAfx.cpp src/StdAfx.h)

add_precompiled_header(vms_gateway src/StdAfx.h FORCEINCLUDE)

target_include_directories(vms_gateway PRIVATE src)

target_link_libraries(vms_gateway libvms_gateway)
