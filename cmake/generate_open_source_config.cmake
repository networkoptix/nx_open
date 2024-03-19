## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/output_directories.cmake)

include(utils)
include(vcs_helpers)

set(UPDATE_SERVER_URL "https://updates.networkoptix.com")
set(SERVER_PLATFORMS windows_x64 linux_x64 linux_arm32 linux_arm64)
set(SERVER_REFS_FILE_NAME "${PROJECT_BINARY_DIR}/compatible_servers.txt")

nx_configure_file("${open_source_root}/build_info.txt.in"
    "${distribution_output_dir}/build_info.txt")
nx_configure_file("${open_source_root}/build_info.json.in"
    "${distribution_output_dir}/build_info.json")
nx_configure_file("${open_source_root}/nx_log_viewer.html" ${CMAKE_BINARY_DIR} COPYONLY)
nx_configure_file("${CMAKE_BINARY_DIR}/conan_refs.txt" ${distribution_output_dir} COPYONLY)

function(nx_generate_compatible_servers_txt)
    if(buildNumber STREQUAL "0")
        file(WRITE "${SERVER_REFS_FILE_NAME}" "# No compatible Server version was detected.")
        nx_store_known_file("${SERVER_REFS_FILE_NAME}")
        return()
    endif()

    set(suffix "")
    nx_vcs_get_meta_release_build_suffix(suffix)

    set(compatible_server_urls "")
    foreach(platform ${SERVER_PLATFORMS})
        set(server_distribution_file_name
            "metavms-server_update-${releaseVersion.full}-${platform}${suffix}.zip")
        list(APPEND compatible_server_urls
            "${UPDATE_SERVER_URL}/metavms/${buildNumber}/${server_distribution_file_name}")
    endforeach()

    list(JOIN compatible_server_urls "\n" compatible_server_urls_string)
    file(WRITE "${SERVER_REFS_FILE_NAME}" "${compatible_server_urls_string}\n")

    if(NOT customization STREQUAL "metavms")
        file(APPEND "${SERVER_REFS_FILE_NAME}"
            "\n# Check the link below for the compatible Server version specific for your "
            "customization.\n# Search for the version ${releaseVersion.full}.\n"
        )
        file(APPEND "${SERVER_REFS_FILE_NAME}" "${UPDATE_SERVER_URL}/${customization}\n")
    endif()

    nx_store_known_file("${SERVER_REFS_FILE_NAME}")
endfunction()
