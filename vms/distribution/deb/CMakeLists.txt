## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include(${open_source_root}/cmake/distribution/deb.cmake)

if(withDesktopClient)
    add_subdirectory(client)
endif()
