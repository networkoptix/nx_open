function(find_python)
    if(WIN32)
        find_package(PythonInterp REQUIRED)
    else()
        find_package(PythonInterp 2 REQUIRED)
    endif()
endfunction()

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
                message(FATAL_ERROR "Cannot detect your system architecture ${CMAKE_SYSTEM_PROCESSOR}")
            endif()
        elseif(WIN32)
            set(detected_platform "windows")
            set(detected_modification "winxp")
            if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
                set(detected_arch "x64")
            elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86")
                set(detected_arch "x86")
            else()
                message(FATAL_ERROR "Cannot detect your system architecture ${CMAKE_SYSTEM_PROCESSOR}")
            endif()
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            set(MACOSX TRUE PARENT_SCOPE)
            set(detected_arch "x64")
            set(detected_platform "macosx")
            set(detected_modification "macosx")
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
            set(LINUX TRUE PARENT_SCOPE)
            set(ANDROID TRUE PARENT_SCOPE)
            set(detected_arch "arm")
            set(detected_platform "android")
            set(detected_modification "")
        else()
            message(FATAL_ERROR "Cannot detect parameters for your system ${CMAKE_SYSTEM_NAME}")
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

function(nx_copy)
    cmake_parse_arguments(COPY "IF_NEWER;IF_DIFFERENT;IF_MISSING" "DESTINATION" "" ${ARGN})

    if(NOT COPY_DESTINATION)
        message(FATAL_ERROR "DESTINATION must be provided.")
    endif()

    if(NOT EXISTS "${COPY_DESTINATION}")
        file(MAKE_DIRECTORY "${COPY_DESTINATION}")
    elseif(NOT IS_DIRECTORY "${COPY_DESTINATION}")
        message(FATAL_ERROR "DESTINATION must be a directory")
    endif()

    foreach(file ${COPY_UNPARSED_ARGUMENTS})
        get_filename_component(dst "${file}" NAME)
        set(dst "${COPY_DESTINATION}/${dst}")

        if(COPY_IF_NEWER)
            file(TIMESTAMP "${file}" orig_ts)
            file(TIMESTAMP "${dst}" dst_ts)
            if(NOT "${orig_ts}" STREQUAL "${dst_ts}")
                message(STATUS "Copying ${file} to ${dst}")
                file(COPY "${file}" DESTINATION "${COPY_DESTINATION}")
            endif()
        elseif(COPY_IF_DIFFERENT)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${file}" "${dst}")
        elseif(COPY_IF_MISSING)
            if(NOT EXISTS "${dst}")
                file(COPY "${file}" DESTINATION "${COPY_DESTINATION}")
            endif()
        else()
            file(COPY "${file}" DESTINATION "${COPY_DESTINATION}")
        endif()
    endforeach()
endfunction()

function(nx_configure_file input output)
    if(IS_DIRECTORY ${output})
        get_filename_component(file_name ${input} NAME)
        set(output "${output}/${file_name}")
    endif()

    message(STATUS "Generating ${output}")
    configure_file(${input} ${output})
endfunction()

function(nx_copy_package package_dir)
    if(EXISTS "${package_dir}/.nocopy")
        return()
    endif()

    if(IS_DIRECTORY "${package_dir}/bin")
        file(GLOB entries "${package_dir}/bin/*")
        foreach(entry ${entries})
            file(COPY "${entry}" DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
        endforeach()
    endif()

    if(LINUX OR MACOSX)
        if(IS_DIRECTORY "${package_dir}/lib")
            if(MACOSX)
                file(GLOB entries "${package_dir}/lib/*.dylib*")
            else()
                file(GLOB entries "${package_dir}/lib/*.so*")
            endif()
            foreach(entry ${entries})
                file(COPY "${entry}" DESTINATION ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
            endforeach()
        endif()
    endif()
endfunction()
