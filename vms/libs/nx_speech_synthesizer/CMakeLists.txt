## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(flite)

nx_add_target(nx_speech_synthesizer LIBRARY NO_API_MACROS
    PUBLIC_LIBS
        nx_vms_common
        flite::flite
    FOLDER common/libs
)

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
