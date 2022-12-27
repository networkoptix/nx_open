## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_subdirectory(client)

# Important: nx_metadata_sdk subdirectory must be added before adding "distribution" subdirectory.
if(withMediaServer OR openSourceBuild)
    add_subdirectory(server)
endif()

if(withDistributions OR withUnitTestsArchive)
    add_subdirectory(distribution)
endif()

add_subdirectory(libs)
