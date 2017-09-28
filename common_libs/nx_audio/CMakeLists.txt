nx_add_target(nx_audio LIBRARY PUBLIC_LIBS common nx_utils)

if(APPLE)
    find_library(OpenAL_LIBRARY OpenAL)
    mark_as_advanced(OpenAL_LIBRARY)
    target_link_libraries(nx_audio PUBLIC ${OpenAL_LIBRARY})
else()
    target_link_libraries(nx_audio PUBLIC openal)
endif()
