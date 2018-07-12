set(default_target_device)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(default_target_device "linux-x64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(default_target_device "windows-x64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(default_target_device "macosx-x64")
endif()
