## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_subdirectory(client)

# Important: nx_metadata_sdk subdirectory must be added before adding "distribution" subdirectory.
if(withMediaServer OR openSourceBuild)
    add_subdirectory(server)
endif()

add_subdirectory(libs)

# Distribution folder should be the last one to ensure all libraries are registered in the global
# lists (e.g. translatable projects list or server plugins list).
if(withDistributions OR withUnitTestsArchive)
    add_subdirectory(distribution)
endif()
