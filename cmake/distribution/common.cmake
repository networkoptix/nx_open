## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

include(CMakeParseArguments)

include(utils)

set(nx_vms_distribution_dir "${open_source_root}/vms/distribution")
set(vms_distribution_common_dir ${nx_vms_distribution_dir}/common)

function(nx_generate_package_json file_name)
    cmake_parse_arguments(
        PACK "" "COMPONENT;VARIANTS;INSTALL_SCRIPT;FREE_SPACE_REQUIRED" "" ${ARGN})
    set(version ${releaseVersion.full})
    set(platform ${platform_new})
    set(cloud_host ${cloudHost})

    if(PACK_COMPONENT)
        set(component ${PACK_COMPONENT})
    else()
        set(component server)
    endif()

    set(variants ${PACK_VARIANTS})

    if(PACK_INSTALL_SCRIPT)
        set(install_script ${PACK_INSTALL_SCRIPT})
    elseif(component STREQUAL server)
        message(FATAL_ERROR "Install script for server must be specified")
    endif()

    if(PACK_FREE_SPACE_REQUIRED)
        set(freeSpaceRequired ${PACK_FREE_SPACE_REQUIRED})
    else()
        set(freeSpaceRequired 0)
    endif()

    nx_configure_file(${nx_vms_distribution_dir}/package.json.in ${file_name})
endfunction()

function(nx_create_distribution_package)
    set(oneValueArgs
        PACKAGE_GENERATION_SCRIPT_NAME
        OUTPUT_FILE
        WORKING_DIRECTORY
        PACKAGE_TARGET
        PACKAGE_NAME)
    set(multiValueArgs
        CONFIG_FILES
        DEPENDENCIES)
    cmake_parse_arguments(DISTRIBUTION "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if("${DISTRIBUTION_CONFIG_FILES}" STREQUAL "")
        message(FATAL_ERROR "No config files passed to nx_create_distribution_package."
            "Arguments passed: ${ARGV}")
    endif()

    list(JOIN DISTRIBUTION_CONFIG_FILES "\;" DISTRIBUTION_CONFIG_FILES_ARG)

    set(package_generation_command
        ${PYTHON_EXECUTABLE} ${DISTRIBUTION_PACKAGE_GENERATION_SCRIPT_NAME}
        "--config" "\"${DISTRIBUTION_CONFIG_FILES_ARG}\""
        "--output_file" ${DISTRIBUTION_OUTPUT_FILE})
    add_custom_command(
        COMMENT "Preparing ${DISTRIBUTION_PACKAGE_NAME} package..."
        DEPENDS
            ${DISTRIBUTION_DEPENDENCIES}
            ${DISTRIBUTION_WORKING_DIRECTORY}/${DISTRIBUTION_PACKAGE_GENERATION_SCRIPT_NAME}
            ${vms_distribution_common_dir}/scripts/distribution_tools.py
        WORKING_DIRECTORY ${DISTRIBUTION_WORKING_DIRECTORY}
        COMMAND ${CMAKE_COMMAND}
            -E env "PYTHONPATH=\"${vms_distribution_common_dir}/scripts\""
            ${package_generation_command}
        OUTPUT ${DISTRIBUTION_OUTPUT_FILE}
    )
    nx_add_targets_to_strengthened(${DISTRIBUTION_OUTPUT_FILE})
    add_custom_target("generate_${DISTRIBUTION_PACKAGE_TARGET}_package" ALL
        DEPENDS ${DISTRIBUTION_OUTPUT_FILE})
endfunction()

function(nx_prepare_build_distribution_conf)
    file(COPY_FILE
        "${open_source_root}/vms/distribution/build_distribution_common.conf.in"
        "${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf.in"
    )

    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/build_distribution.conf.in" SPECIFIC_CONF_CONTENTS)
    file(APPEND
        "${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf.in"
        "${SPECIFIC_CONF_CONTENTS}"
    )

    nx_configure_file(
        "${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf.in"
        "${CMAKE_CURRENT_BINARY_DIR}/build_distribution.conf"
        @ONLY
    )
endfunction()
