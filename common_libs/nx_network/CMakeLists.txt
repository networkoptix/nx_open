find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

add_library(nx_network
    ${CPP_FILES} ${MOC_FILES}
    src/StdAfx.cpp src/StdAfx.h
)

add_precompiled_header(nx_network src/StdAfx.h FORCEINCLUDE)

target_include_directories(nx_network PUBLIC src)

target_compile_definitions(nx_network
    PRIVATE NX_NETWORK_API=${API_EXPORT_MACRO}
    INTERFACE NX_NETWORK_API=${API_IMPORT_MACRO}
)

target_link_libraries(nx_network PUBLIC nx_streaming udt)
