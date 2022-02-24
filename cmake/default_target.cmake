## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set(default_target_device)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(default_target_device "linux_x64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(default_target_device "windows_x64")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    # Workaround for missing CMAKE_HOST_SYSTEM_PROCESSOR, see https://github.com/conan-io/conan/issues/8025
    execute_process(COMMAND uname -p OUTPUT_VARIABLE proc_arch OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(proc_arch STREQUAL "arm")
        set(default_target_device "macos_arm64")
    else()
        set(default_target_device "macos_x64")
    endif()
endif()
