function(nx_strip_target target)
    get_target_property(type ${target} TYPE)
    if(NOT type MATCHES "SHARED_LIBRARY|EXECUTABLE")
        return()
    endif()

    cmake_parse_arguments(STRIP "ONLY_DEBUG_INFO;COPY_DEBUG_INFO" "" "" ${ARGN})

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(NOT CMAKE_STRIP)
            message(FATAL_ERROR "strip is not found.")
        endif()

        if(STRIP_COPY_DEBUG_INFO)
            if(NOT CMAKE_BUILD_TYPE MATCHES "Default|Debug|RelWithDebInfo")
                set(STRIP_COPY_DEBUG_INFO)
            endif()
        endif()

        set(strip_flags --strip-debug)

        if(NOT STRIP_ONLY_DEBUG_INFO)
            list(APPEND strip_flags --strip-unneeded)
        endif()

        if(STRIP_COPY_DEBUG_INFO)
            if(NOT CMAKE_OBJCOPY)
                message(FATAL_ERROR "objcopy is not found.")
            endif()

            set(copy_debug_info_command COMMAND
                ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:${target}>
                $<TARGET_FILE:${target}>.debug)

            set(add_debuglink_command COMMAND
                ${CMAKE_OBJCOPY} --add-gnu-debuglink=$<TARGET_FILE:${target}>.debug
                $<TARGET_FILE:${target}>)
        else()
            set(copy_debug_info_command)
            set(add_debuglink_command)
        endif()

        add_custom_command(TARGET ${target} POST_BUILD
            ${copy_debug_info_command}
            COMMAND ${CMAKE_STRIP} ${strip_flags} $<TARGET_FILE:${target}>
            ${add_debuglink_command}
            COMMENT "Stripping ${target}")
    endif()
endfunction()
