rdep_add_package(festival)
rdep_add_package(any/festival-vox)

nx_add_target(nx_speech_synthesizer LIBRARY PUBLIC_LIBS common)

target_include_directories(nx_speech_synthesizer PRIVATE ${FESTIVAL_HEADERS})
