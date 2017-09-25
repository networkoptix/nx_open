set(artifact.name.server_update ${artifact.name.server_update})
file(GLOB_RECURSE server_update_files
    "${CMAKE_CURRENT_BINARY_DIR}/server/*"
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/exe/*server*.exe"
)