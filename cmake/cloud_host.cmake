set(cloudGroup "test" CACHE STRING "Cloud instance group (dev, test, demo, prod)")
set(customCloudHost "" CACHE STRING "Cloud host. Leave empty to fetch cloud host from server.")

if("${customCloudHost}" STREQUAL "")
    set(url "http://ireg.hdw.mx/api/v1/cloudhostfinder/?group=${cloudGroup}&vms_customization=${customization}")
    message(STATUS "Fetching cloud host from ${url}")
    set(_cloud_tmp_file "${CMAKE_CURRENT_BINARY_DIR}/cloud_host.tmp")
    file(DOWNLOAD ${url} ${_cloud_tmp_file} STATUS _status)
    file(READ ${_cloud_tmp_file} cloudHost)
    file(REMOVE ${_cloud_tmp_file})
    unset(_cloud_tmp_file)
    if("${cloudHost}" STREQUAL "")
        message(FATAL_ERROR "Could not fetch cloud host from the server.\nStatus: ${_status}")
    endif()
    unset(_status)
else()
    message(STATUS "Using the provided cloud host ${customCloudHost}")
    set(cloudHost ${customCloudHost})
endif()
