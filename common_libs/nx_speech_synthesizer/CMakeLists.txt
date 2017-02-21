rdep_add_package(festival)
rdep_add_package(any/festival-vox)

nx_add_target(nx_speech_synthesizer LIBRARY PUBLIC_LIBS common)

if(NOT LINUX)
    target_link_libraries(nx_speech_synthesizer PRIVATE festival)
else()
    target_include_directories(nx_speech_synthesizer PRIVATE ${FESTIVAL_HEADERS})
endif()
