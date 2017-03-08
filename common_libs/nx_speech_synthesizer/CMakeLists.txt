rdep_add_package(festival)
if(NOT "${festival-vox_version}" STREQUAL "system")
    rdep_add_package(any/festival-vox)
endif()

nx_add_target(nx_speech_synthesizer LIBRARY
    PUBLIC_LIBS common
    PRIVATE_LIBS festival)
