## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

nx_configure_file(
    "${open_source_root}/build_info.txt"
    ${CMAKE_CURRENT_BINARY_DIR}/distrib)
nx_configure_file(
    "${open_source_root}/build_info.json"
    ${CMAKE_CURRENT_BINARY_DIR}/distrib)
nx_configure_file(
    "${open_source_root}/nx_log_viewer.html"
    ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
