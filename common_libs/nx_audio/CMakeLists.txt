nx_add_target(nx_audio LIBRARY PUBLIC_LIBS nx_streaming)

if(LINUX)
    target_link_libraries(nx_audio PUBLIC openal)
endif()

if(MACOSX)
    find_library(OpenAL_LIBRARY OpenAL)
    mark_as_advanced(OpenAL_LIBRARY)
    target_link_libraries(nx_audio PUBLIC ${OpenAL_LIBRARY})
endif()
