rdep_add_package(festival)

nx_add_target(nx_speech_synthesizer LIBRARY PUBLIC_LIBS common)

target_include_directories(nx_speech_synthesizer PRIVATE ${FESTIVAL_HEADERS})
