find_sources(src CPP_FILES HPP_FILES)
qt5_wrap_cpp(MOC_FILES ${HPP_FILES})

add_library(nx_media
    ${CPP_FILES} ${MOC_FILES}
    src/StdAfx.cpp src/StdAfx.h
)

add_precompiled_header(nx_media src/StdAfx.h FORCEINCLUDE)

target_include_directories(nx_media
    PRIVATE
        ${Qt5Multimedia_PRIVATE_INCLUDE_DIRS}
    PUBLIC src
)

target_link_libraries(nx_media PUBLIC nx_audio)
