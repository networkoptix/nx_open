function(detect_platform)
    if(box STREQUAL "none")
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(LINUX TRUE PARENT_SCOPE)
            set(detected_platform "linux")
            set(detected_modification "ubuntu")
            if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
                set(detected_arch "x64")
            elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86")
                set(detected_arch "x86")
            else()
                message(FATAL_ERROR "Cannot detect your system architecture" ${CMAKE_SYSTEM_PROCESSOR})
            endif()
        elseif(WIN32)
            set(detected_platform "windows")
            set(detected_modification "winxp")
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
            set(detected_modification "macosx")
        else()
            message(FATAL_ERROR "Cannot detect parameters for your system" ${CMAKE_SYSTEM_NAME})
        endif()

        set(detected_target ${detected_platform}-${detected_arch})
    else()
        set(detected_arch "arm")
        set(detected_platform "linux")
        set(detected_modification ${box})

        if(box STREQUAL "bananapi")
            set(detected_target "bpi")
        else()
            set(detected_target ${box})
        endif()

        if(detected_target STREQUAL "bpi")
            set(BPI TRUE PARENT_SCOPE)
        elseif(detected_target STREQUAL "rpi")
            set(RPI TRUE PARENT_SCOPE)
        elseif(detected_target STREQUAL "isd")
            set(ISD TRUE PARENT_SCOPE)
        elseif(detected_target STREQUAL "isd_s2")
            set(ISD_S2 TRUE PARENT_SCOPE)
        endif()
    endif()

    set(arch ${detected_arch} CACHE INTERNAL "")
    set(platform ${detected_platform} CACHE INTERNAL "")
    set(modification ${detected_modification} CACHE INTERNAL "")
    set(rdep_target ${detected_target} CACHE INTERNAL "")
endfunction()

function(nx_copy_if_different file destination)
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${file} ${destination})
endfunction()

function(nx_files_differ file1 file2 var)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E compare_files ${file1} ${file2}
        OUTPUT_QUIET ERROR_QUIET
        RESULT_VARIABLE result)
    set(${var} ${result} PARENT_SCOPE)
endfunction()

function(nx_configure_file input output)
    if(IS_DIRECTORY ${output})
        get_filename_component(file_name ${input} NAME)
        set(output "${output}/${file_name}")
    endif()

    set(copy "${output}.copy")

    configure_file(${input} ${copy})

    nx_files_differ(${copy} ${output} result)
    if(result)
        file(RENAME ${copy} ${output})
    else()
        file(REMOVE ${copy})
    endif()
endfunction()
