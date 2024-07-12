## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(project bytetrack)
set(project_dir ${open_source_root}/artifacts/${project}/src)

find_package(Eigen3 REQUIRED)

file(GLOB sources CONFIGURE_DEPENDS "${project_dir}/${project}/*.cpp")
file(GLOB headers CONFIGURE_DEPENDS "${project_dir}/${project}/*.h")

add_library(${project} STATIC ${headers} ${sources})
target_link_libraries(${project} PUBLIC Eigen3::Eigen nx_utils)

set_target_properties(${project} PROPERTIES FOLDER third_party)

target_include_directories(${project} PUBLIC ${project_dir})
