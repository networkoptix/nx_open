nx_add_target(nx_audio LIBRARY PUBLIC_LIBS nx_streaming)

if(LINUX)
    if(ANDROID)
        rdep_add_package(openal)
    endif()
    target_link_libraries(nx_audio PUBLIC openal)
endif()

if(WIN32)
    rdep_add_package(openal)
    target_link_libraries(nx_audio PUBLIC openal)
endif()

if(MACOSX)
    find_library(OpenAL_LIBRARY OpenAL)
    mark_as_advanced(OpenAL_LIBRARY)
    target_link_libraries(nx_audio PUBLIC ${OpenAL_LIBRARY})
endif()
