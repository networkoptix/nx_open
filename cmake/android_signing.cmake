## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

function(find_apksigner)
    if(CMAKE_HOST_WIN32)
        set(apksigner_suffix ".bat")
    else()
        set(apksigner_suffix "")
    endif()

    set(build_tools_path ${CONAN_ANDROIDSDK_ROOT}/build-tools)
    file(GLOB_RECURSE FOUND_FILES "${build_tools_path}/**/apksigner${apksigner_suffix}")
    if(FOUND_FILES)
        list(GET FOUND_FILES 0 APKSIGNER)
    else()
        message(FATAL_ERROR "apksigner${apksigner_suffix} not found in ${build_tools_path}")
    endif()

    set(APKSIGNER ${APKSIGNER} PARENT_SCOPE)
endfunction()

# Opensource Android code signing requires several environment variables set:
# - ANDROID_KEY_STORE - path to the android key store
# - ANDROID_KEY_STORE_PASS - password for the key store
# - ANDROID_KEY_STORE_ALIAS - alias of the key in the key store
# - ANDROID_KEY_STORE_KEYPASS - password for the key

function(nx_get_android_sign_command variable source_file output_file)
    if(NOT APKSIGNER)
        find_apksigner()
    endif()

    if(NOT DEFINED ENV{ANDROID_KEY_STORE})
        message(FATAL_ERROR "ANDROID_KEY_STORE is not set")
    endif()

    if(NOT DEFINED ENV{ANDROID_KEY_STORE_PASS})
        message(FATAL_ERROR "ANDROID_KEY_STORE_PASS is not set")
    endif()

    if(NOT DEFINED ENV{ANDROID_KEY_STORE_ALIAS})
        message(FATAL_ERROR "ANDROID_KEY_STORE_ALIAS is not set")
    endif()

    if(NOT DEFINED ENV{ANDROID_KEY_STORE_KEYPASS})
        message(FATAL_ERROR "ANDROID_KEY_STORE_KEYPASS is not set")
    endif()

    set(sign_command
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${source_file} ${output_file}
        COMMAND ${APKSIGNER}
            sign
            --ks $ENV{ANDROID_KEY_STORE}
            -ks-key-alias $ENV{ANDROID_KEY_STORE_ALIAS}
            --ks-pass env:ANDROID_KEY_STORE_PASS
            --key-pass env:ANDROID_KEY_STORE_KEYPASS
            ${output_file}
        COMMAND ${CMAKE_COMMAND} -E remove ${output_file}.idsig
    )
    set(${variable} ${sign_command} PARENT_SCOPE)
endfunction()
