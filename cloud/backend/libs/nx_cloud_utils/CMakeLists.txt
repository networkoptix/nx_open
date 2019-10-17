add_library(nx_cloud_utils INTERFACE)
target_include_directories(nx_cloud_utils INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src)

target_link_libraries(nx_cloud_utils INTERFACE nx_network nx_utils)
