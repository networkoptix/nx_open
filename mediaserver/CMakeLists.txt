rdep_add_package(any/server-external-${branch} OPTIONAL PATH_VARIABLE server_external_path)
if(NOT server_external_path)
    rdep_add_package(any/server-external-${releaseVersion})
endif()

nx_add_target(mediaserver EXECUTABLE
    PUBLIC_LIBS mediaserver_core festival)
