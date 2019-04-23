nx_configure_file(
    "${PROJECT_SOURCE_DIR}/build_info.txt"
    ${CMAKE_CURRENT_BINARY_DIR})
nx_configure_file(
    "${PROJECT_SOURCE_DIR}/current_config.py"
    ${CMAKE_CURRENT_BINARY_DIR})
nx_configure_file(
    "${PROJECT_SOURCE_DIR}/specific_features.txt"
    ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
nx_configure_file(
    "${PROJECT_SOURCE_DIR}/nx_log_viewer.html"
    ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
