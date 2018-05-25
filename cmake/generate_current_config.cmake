# Looks like we need to combine these files into one
nx_configure_file(
    "${PROJECT_SOURCE_DIR}/build_info.txt"
    ${CMAKE_CURRENT_BINARY_DIR})
nx_configure_file(
    "${PROJECT_SOURCE_DIR}/current_config.py"
    ${CMAKE_CURRENT_BINARY_DIR})
