find_sources(src CPP_FILES)

add_library(nx_speech_synthesizer
    ${CPP_FILES}
    src/StdAfx.cpp src/StdAfx.h
)

add_precompiled_header(nx_speech_synthesizer src/StdAfx.h FORCEINCLUDE)

rdep_add_package(festival)

target_include_directories(nx_speech_synthesizer
    PRIVATE ${FESTIVAL_HEADERS}
    PUBLIC src
)

target_link_libraries(nx_speech_synthesizer PUBLIC common)
