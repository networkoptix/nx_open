rdep_add_package(festival)
rdep_add_package(any/festival-vox)

nx_add_target(nx_speech_synthesizer LIBRARY
    PUBLIC_LIBS common
    PRIVATE_LIBS festival)
