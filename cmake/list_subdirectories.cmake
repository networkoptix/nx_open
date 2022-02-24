## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard()

# Get the list of subdirectories under to <cur_dir>. Provides relative paths by default.
#
# Args:
#  - cur_dir: the directory to search for subdirectories
#  - out_var: the output variable containing the list of subdirectories.
#  - ABSOLUTE (optional): if specified, the subdirectories will be full paths.
#  - RECURSE (optional): if specified, a depth first search of all subdirectories is done and
#       added to out_var.
#
# Usage:
#  - list_subdirectories(${CMAKE_CURRENT_SOURCE_DIR} my_list_var [ABSOLUTE] [RECURSE])
function(list_subdirectories cur_dir out_var)
    set(options ABSOLUTE RECURSE)
    cmake_parse_arguments(LIST_SUBDIR "${options}" "" "" ${ARGN})

    if(LIST_SUBDIR_RECURSE)
        set(recurse RECURSE)
    endif()

    list_subdirectories_impl(${cur_dir} dirs ${recurse})

    if(LIST_SUBDIR_ABSOLUTE)
        set(${out_var} ${dirs} PARENT_SCOPE)
        return()
    endif()

    set(result)
    foreach(dir ${dirs})
        string(REPLACE "${cur_dir}/" "" relative_dir ${dir})
        list(APPEND result ${relative_dir})
    endforeach()

    set(${out_var} ${result} PARENT_SCOPE)
endfunction()

function(list_subdirectories_impl cur_dir out_var)
    set(options RECURSE)
    cmake_parse_arguments(LIST_SUBDIR "${options}" "" "" ${ARGN})
    file(GLOB sub_dirs CONFIGURE_DEPENDS RELATIVE ${cur_dir} ${cur_dir}/*)

    set(list_of_dirs)
    foreach(dir ${sub_dirs})
        if(IS_DIRECTORY ${cur_dir}/${dir})
            list(APPEND list_of_dirs ${cur_dir}/${dir})
            if(LIST_SUBDIR_RECURSE)
                list_subdirectories_impl(${cur_dir}/${dir} temp RECURSE)
                list(APPEND list_of_dirs ${temp})
            endif()
        endif()
    endforeach()

    set(${out_var} ${list_of_dirs} PARENT_SCOPE)
endfunction()
