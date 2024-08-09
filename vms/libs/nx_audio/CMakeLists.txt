## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

find_package(Qt6 COMPONENTS Multimedia REQUIRED)

if (IOS)
    set(mac_sources src/nx/audio/audio_ios.mm)
endif()

nx_add_target(nx_audio LIBRARY NO_API_MACROS
    ADDITIONAL_SOURCES
        ${mac_sources}
    PUBLIC_LIBS
        Qt6::Multimedia
        nx_vms_common
        nx_utils
    FOLDER client/libs
)

if(APPLE)
    find_library(OpenAL_LIBRARY_NAME OpenAL REQUIRED)
    mark_as_advanced(OpenAL_LIBRARY_NAME)
else()
    find_package(openal REQUIRED)
    set(OpenAL_LIBRARY_NAME openal::openal)
endif()

target_link_libraries(nx_audio PUBLIC ${OpenAL_LIBRARY_NAME})
