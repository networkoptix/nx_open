set(libs common)

if(NOT targetDevice STREQUAL "edge1")
    list(APPEND libs festival)
endif()

nx_add_target(nx_speech_synthesizer LIBRARY
    PUBLIC_LIBS ${libs})

if(MACOSX)
    find_library(CoreServices_LIBRARY CoreServices)
    mark_as_advanced(CoreServices_LIBRARY)

    find_library(IOKit_LIBRARY IOKit)
    mark_as_advanced(IOKit_LIBRARY)

    find_library(AudioToolbox_LIBRARY AudioToolbox)
    mark_as_advanced(AudioToolbox_LIBRARY)

    target_link_libraries(nx_speech_synthesizer PUBLIC ${CoreServices_LIBRARY} ${IOKit_LIBRARY} ${AudioToolbox_LIBRARY})
endif()
