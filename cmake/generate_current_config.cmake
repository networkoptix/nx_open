nx_configure_file(
    "${PROJECT_SOURCE_DIR}/build_info.txt"
    ${CMAKE_CURRENT_BINARY_DIR})
nx_configure_file(
    "${PROJECT_SOURCE_DIR}/build_info.txt"
    ${CMAKE_CURRENT_BINARY_DIR}/distrib)
nx_configure_file(
    "${PROJECT_SOURCE_DIR}/current_config.py"
    ${CMAKE_CURRENT_BINARY_DIR})
nx_configure_file(
    "${PROJECT_SOURCE_DIR}/specific_features.txt"
    ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)

