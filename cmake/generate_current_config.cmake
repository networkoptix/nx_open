# Use "take local" when will merge that in to 3.2. Backport.
nx_configure_file(
    "${PROJECT_SOURCE_DIR}/build_info.txt"
    ${CMAKE_CURRENT_BINARY_DIR})
nx_configure_file(
    "${PROJECT_SOURCE_DIR}/build_variables/maven/filter-resources/current_config.py"
    "${CMAKE_CURRENT_BINARY_DIR}")
