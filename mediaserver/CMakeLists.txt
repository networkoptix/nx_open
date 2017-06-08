nx_add_target(mediaserver EXECUTABLE
    PUBLIC_LIBS mediaserver_core)

if(TARGET festival_libs)
    target_link_libraries(mediaserver PRIVATE festival_libs)
endif()

project(mediaserver)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT mediaserver)

