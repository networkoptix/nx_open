find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

add_library(nx_audio
    ${CPP_FILES} ${MOC_FILES}
    src/StdAfx.cpp src/StdAfx.h
)

add_precompiled_header(nx_audio src/StdAfx.h FORCEINCLUDE)

target_include_directories(nx_audio PUBLIC src)

target_link_libraries(nx_audio PUBLIC nx_streaming openal)
