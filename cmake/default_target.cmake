set(default_target_device)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(default_target_device "linux-x64")
endif()
