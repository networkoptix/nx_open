## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(libs nx_vms_common)

set(_enableSpeechSynthesizer ON)

if(_enableSpeechSynthesizer)
    find_package(flite)
    list(APPEND libs flite::flite)
endif()

nx_add_target(nx_speech_synthesizer LIBRARY NO_API_MACROS
    PUBLIC_LIBS ${libs}
    FOLDER common/libs
)

if(NOT _enableSpeechSynthesizer)
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/nx/speech_synthesizer/text_to_wave_server.cpp
        PROPERTIES COMPILE_FLAGS -DDISABLE_FLITE)
endif()

unset(_enableSpeechSynthesizer)

if(MACOSX)
    find_library(CoreServices_LIBRARY CoreServices)
    mark_as_advanced(CoreServices_LIBRARY)

    find_library(IOKit_LIBRARY IOKit)
    mark_as_advanced(IOKit_LIBRARY)

    find_library(AudioToolbox_LIBRARY AudioToolbox)
    mark_as_advanced(AudioToolbox_LIBRARY)

    target_link_libraries(nx_speech_synthesizer
        PUBLIC
            ${CoreServices_LIBRARY}
            ${IOKit_LIBRARY}
            ${AudioToolbox_LIBRARY}
    )
endif()
