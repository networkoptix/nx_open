function(detect_target)
    if(NOT targetDevice OR targetDevice STREQUAL "linux-x86")
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(LINUX TRUE PARENT_SCOPE)
            set(detected_platform "linux")
            set(detected_modification "ubuntu")
            if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
                set(detected_arch "x64")
            elseif(CMAKE_SYSTEM_PROCESSOR MATCHES ".*86")
                set(detected_arch "x86")
            else()
                message(FATAL_ERROR
                    "Cannot detect your system architecture ${CMAKE_SYSTEM_PROCESSOR}")
            endif()
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
            set(WINDOWS TRUE PARENT_SCOPE)
            set(detected_platform "windows")
            set(detected_modification "winxp")
            if(MSVC)
                if(CMAKE_CL_64)
                    set(detected_arch "x64")
                else()
                    set(detected_arch "x86")
                endif()
            else()
                if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
                    set(detected_arch "x64")
                elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL ".*86")
                    set(detected_arch "x86")
                else()
                    message(FATAL_ERROR
                        "Cannot detect your system architecture ${CMAKE_SYSTEM_PROCESSOR}")
                endif()
            endif()
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            set(MACOSX TRUE PARENT_SCOPE)
            set(detected_arch "x64")
            set(detected_platform "macosx")
            set(detected_modification "macosx")
        else()
            message(FATAL_ERROR
                "Cannot detect parameters for your system ${CMAKE_SYSTEM_NAME}")
        endif()

        set(detected_rdep_target ${detected_platform}-${detected_arch})
    else()
        if(targetDevice STREQUAL "bananapi")
            set(detected_rdep_target "bpi")
        elseif(targetDevice STREQUAL "edge1")
            set(detected_rdep_target "isd_s2")
        elseif(targetDevice STREQUAL "tx1-aarch64")
            set(detected_rdep_target "tx1-aarch64")
        else()
            set(detected_rdep_target ${targetDevice})
        endif()

        if(targetDevice MATCHES "android")
            set(LINUX TRUE PARENT_SCOPE)
            set(ANDROID TRUE PARENT_SCOPE)
            set(detected_arch "arm")
            set(detected_platform "android")
            set(detected_modification "")
        elseif(targetDevice MATCHES "ios")
            set(detected_arch "arm")
            set(detected_platform "ios")
            set(detected_modification "")
            set(detected_rdep_target "ios")
        elseif(targetDevice MATCHES "tx1")
            set(LINUX TRUE PARENT_SCOPE)
            set(detected_arch "aarch64")
            set(detected_platform "linux")
            set(detected_modification ${box})
        else()
            set(LINUX TRUE PARENT_SCOPE)
            set(detected_arch "arm")
            set(detected_platform "linux")
            set(detected_modification ${box})
        endif()
    endif()

    set(arch ${detected_arch} CACHE INTERNAL "")
    set(platform ${detected_platform} CACHE INTERNAL "")
    set(modification ${detected_modification} CACHE INTERNAL "")
    set(rdep_target ${detected_rdep_target} CACHE INTERNAL "")
endfunction()

detect_target()
