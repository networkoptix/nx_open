set(detours_dir ${CMAKE_SOURCE_DIR}/artifacts/detours/src)

file(GLOB sources ${detours_dir}/*.cpp)
list(REMOVE_ITEM sources ${detours_dir}/uimports.cpp)

add_library(detours STATIC ${sources})
set_target_properties(detours PROPERTIES FOLDER third_party)

target_include_directories(detours PUBLIC ${detours_dir})
