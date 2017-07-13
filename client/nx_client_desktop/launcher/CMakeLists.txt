cmake_minimum_required(VERSION 2.8.12)

if(NOT CONFIG_DIRECTORY)
    message(FATAL_ERROR "CONFIG_DIRECTORY is not specified.")
endif()

add_definitions(-DUNICODE -D_CONSOLE -D_UNICODE)
include_directories("${CONFIG_DIRECTORY}")
add_executable(launcher src/main.cpp src/progress.cpp src/launcher.rc)
set_target_properties(launcher PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
target_link_libraries(launcher msi comctl32)

install(TARGETS launcher RUNTIME DESTINATION bin)
