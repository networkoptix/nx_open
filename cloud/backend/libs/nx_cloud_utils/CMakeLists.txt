file(GLOB_RECURSE nx_cloud_utils_cpp src/*.cpp)
if(nx_cloud_utils_cpp)
    message(FATAL_ERROR "nx_cloud_utils is configured as header only but has .cpp files! Configure with nx_add_target().")
endif()

# At the moment there is only a header file so nx_cloud_utils needs to be a header only library.
# If .cpp files are added then header only will be incorrect.

add_library(nx_cloud_utils INTERFACE)
target_include_directories(nx_cloud_utils INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(nx_cloud_utils INTERFACE nx_network nx_utils)
