function(detect_platform)
    if(box STREQUAL "none")
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(detected_platform "linux")
            if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
                set(detected_arch "x64")
            elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86")
                set(detected_arch "x86")
            else()
                message(FATAL_ERROR "Cannot detect your system architecture" ${CMAKE_SYSTEM_PROCESSOR})
            endif()
        elseif(WIN32)
            set(detected_platform "windows")
            if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
                set(detected_arch "x64")
            elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
                set(detected_arch "x86")
            else()
                message(FATAL_ERROR "Cannot detect your system architecture" ${CMAKE_SYSTEM_PROCESSOR})
            endif()
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            set(detected_arch "x64")
            set(detected_platform "macosx")
        else()
            message(FATAL_ERROR "Cannot detect parameters for your system" ${CMAKE_SYSTEM_NAME})
        endif()
            set(detected_target ${detected_platform}-${detected_arch})
    else()
        set(detected_arch "arm")
        set(detected_platform "linux")
        set(detected_target ${box})
    endif()

    set(arch ${detected_arch} CACHE INTERNAL "")
    set(platform ${detected_platform} CACHE INTERNAL "")
    set(rdep_target ${detected_target} CACHE INTERNAL "")
endfunction()
