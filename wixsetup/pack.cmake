list(APPEND CMAKE_MODULE_PATH
    ${PROJECT_SOURCE_DIR}/build_utils/cmake
    ${PROJECT_SOURCE_DIR}/cmake)

include(utils)
include(mercurial)

file(GLOB filesets_to_process ${CMAKE_CURRENT_BINARY_DIR}/packages/*.cmake)

foreach(fileset ${filesets_to_process})
    get_filename_component(fileset_name ${fileset} NAME_WE)
    message(STATUS "Processing ${fileset_name}")
    include(${CMAKE_CURRENT_BINARY_DIR}/packages/${fileset_name}.cmake)
    message(STATUS "Creating ${artifact.name.${fileset_name}}")
    foreach(item ${${fileset_name}_files})
        nx_copy("${item}"
            DESTINATION "${target_pack_dir}/${artifact.name.${fileset_name}}"
            IF_NEWER IF_DIFFERENT
        )
    endforeach()
endforeach()