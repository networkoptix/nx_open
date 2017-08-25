set(libs common)

if(NOT targetDevice STREQUAL "edge1")
    list(APPEND list festival)
endif()

nx_add_target(nx_speech_synthesizer LIBRARY
    PUBLIC_LIBS ${libs})
