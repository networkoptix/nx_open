set(stackwalker_dir ${CMAKE_SOURCE_DIR}/artifacts/stackwalker/Main/StackWalker)

file(GLOB sources ${stackwalker_dir}/*.cpp)

add_definitions(-D_UNICODE)

add_library(stackwalker STATIC ${sources})
set_target_properties(stackwalker PROPERTIES FOLDER third_party)

target_include_directories(stackwalker PUBLIC ${stackwalker_dir})
