set(depends mediaserver)
if(targetDevice STREQUAL "bpi")
    list(APPEND depends "mobile_client")
endif()

set(tar_filename ${artifact.name.server}.tar.gz)
set(zip_filename ${artifact.name.server_update}.zip)
set(installer_output_list ${tar_filename} ${zip_filename})

# Set variables which may depend on current project and are used by create_arm_installer.conf.
set(project.build.directory ${CMAKE_CURRENT_BINARY_DIR})
set(build.configuration "") #< Used by create_arm_installer.sh as suffix to lib and bin dirs.

set(toolchain_lib_directory)
if(arch STREQUAL "arm")
    set(toolchain_lib_directory "${toolchain_directory}/arm-unknown-linux-gnueabihf/sysroot/lib")
endif()

set(conf_file maven/filter-resources/create_arm_installer.conf)
nx_configure_file(
    ${conf_file}
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY)
list(APPEND depends ${conf_file})

function(configure_directory input output)
    cmake_parse_arguments(ARG "" "OUTPUT_FILES_VARIABLE" "" ${ARGN})

    set(output_files)

    message(STATUS "Configuring directory (recursively) ${input}")
    file(GLOB_RECURSE files RELATIVE ${input} ${input}/*)
    foreach(file ${files})
        set(src_template_path ${input}/${file})
        if(NOT IS_DIRECTORY ${src_template_path})
            nx_configure_file(${input}/${file} ${output}/${file} ${ARG_UNPARSED_ARGUMENTS})
            list(APPEND output_files ${output}/${file})
        endif()
    endforeach()

    if(ARG_OUTPUT_FILES_VARIABLE)
        set(${ARG_OUTPUT_FILES_VARIABLE} ${output_files} PARENT_SCOPE)
    endif()
endfunction()

if(targetDevice STREQUAL "edge1")
    set(box_dir "isd")
else()
    set(box_dir "rpi")
endif()

configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/${box_dir}/maven/filter-resources
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE filtered1)

configure_directory(
    ${CMAKE_CURRENT_SOURCE_DIR}/${box_dir}/maven/${targetDevice}
    ${CMAKE_CURRENT_BINARY_DIR}
    @ONLY
    OUTPUT_FILES_VARIABLE filtered2)

set(script ${CMAKE_CURRENT_SOURCE_DIR}/maven/bin-resources/create_arm_installer.sh)

add_custom_command(
    OUTPUT ${installer_output_list}
    COMMAND ${script}
    DEPENDS ${script} ${depends} ${filtered1} ${filtered2}
    COMMENT "Creating ARM installer archives"
)

add_custom_target(arm-installer ALL
    DEPENDS ${installer_output_list}
)
