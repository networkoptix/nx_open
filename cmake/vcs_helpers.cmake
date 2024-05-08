## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

# ATTENTION: These expressions are valid only for single-digit VMS version numbers.
set(META_RELEASE_TAG_GLOB "vms/[0-9].[0-9]*/[0-9]*_meta_*")
set(META_RELEASE_TAG_REGEX "^vms/[0-9]\.[0-9](\.[0-9])?/([0-9]+)(_beta|_rc)?.*_meta_.+$")
set(PROTECTED_BRANCH_REGEX "^(master$|vms_[0-9]\.[0-9](\.[0-9])?(_patch)?$)")

# The BUILD_NUMBER argument can be omitted - in this case we do not try to obtain the compatible
# Server build number using the algorithm described in the comment to _extract_git_info().
function(nx_vcs_get_info)
    cmake_parse_arguments(GIT "" "CHANGESET;BRANCH;CURRENT_REFS;BUILD_NUMBER" "" ${ARGN})

    if(developerBuild)
        set(_changeset "UNKNOWN")
        set(_branch "UNKNOWN")
        set(_current_refs "UNKNOWN")
    else()
        # For a non-developer build leaving the default value for any of these variables leads to
        # the fatal error.
        set(_changeset "")
        set(_branch "")
        set(_current_refs "")
    endif()

    set(_build_number "0")

    if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/git_info.txt)
        message("ATTENTION: Using git information from ${CMAKE_CURRENT_LIST_DIR}/git_info.txt.")
        _read_git_info(
            CHANGESET _changeset
            BRANCH _branch
            CURRENT_REFS _current_refs
            BUILD_NUMBER ${GIT_BUILD_NUMBER}
        )
    else()
        execute_process(
            COMMAND git -C "${CMAKE_CURRENT_LIST_DIR}" rev-parse --is-inside-work-tree
            OUTPUT_VARIABLE _result
            RESULT_VARIABLE _return_code
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(_return_code EQUAL 0 AND _result STREQUAL true)
            _extract_git_info(
                CHANGESET _changeset
                BRANCH _branch
                CURRENT_REFS _current_refs
                BUILD_NUMBER ${GIT_BUILD_NUMBER}
            )
        else()
            message(
                "ATTENTION: ${CMAKE_CURRENT_LIST_DIR} is not inside the repository working tree "
                "and doesn't contain git_info.txt.")
        endif()
    endif()

    set(${GIT_CHANGESET} ${_changeset} PARENT_SCOPE)
    set(${GIT_BRANCH} ${_branch} PARENT_SCOPE)
    set(${GIT_CURRENT_REFS} ${_current_refs} PARENT_SCOPE)
    if(GIT_BUILD_NUMBER)
        set(${GIT_BUILD_NUMBER} ${${GIT_BUILD_NUMBER}} PARENT_SCOPE)
    endif()
endfunction()

# Git info file inherits the format from build_info.txt, but contains only info obtained from git,
# i.e. parameters `changeSet=`, `branch=`, and `currentRefs=`, in this particular order. It also
# can contain `buildNumber=` (on the next string after `currentRefs=`), which is not present in
# build_info.txt. Note that the BUILD_NUMBER argument of this function can be omitted - in this
# case the function does not try to read the value of `buildNumber=` from the file.
#
# build_info.txt is expected to be generated externally when building without the git repo
# available (e.g. on a remote host).
function(_read_git_info)
    cmake_parse_arguments(GIT "" "CHANGESET;BRANCH;CURRENT_REFS;BUILD_NUMBER" "" ${ARGN})

    set(file ${CMAKE_CURRENT_LIST_DIR}/git_info.txt)
    set(_git_info "")

    file(STRINGS ${file} _git_info) #< On error, _git_info remains empty.

    list(GET _git_info 0 _changeset_line)
    if(_changeset_line MATCHES "^changeSet=(.+)$")
        set(${GIT_CHANGESET} ${CMAKE_MATCH_1} PARENT_SCOPE)
    endif()

    list(GET _git_info 1 _branch_line)
    if(_branch_line MATCHES "^branch=(.+)$")
        set(${GIT_BRANCH} ${CMAKE_MATCH_1} PARENT_SCOPE)
    endif()

    list(GET _git_info 2 _current_refs_line)
    if(_current_refs_line MATCHES "^currentRefs=(.+)$")
        set(${GIT_CURRENT_REFS} ${CMAKE_MATCH_1} PARENT_SCOPE)
    endif()

    if(GIT_BUILD_NUMBER)
        list(GET _git_info 3 _build_number_line)
        if(_build_number_line MATCHES "^buildNumber=(.+)$")
            set(${GIT_BUILD_NUMBER} ${CMAKE_MATCH_1} PARENT_SCOPE)
        else()
            set(${GIT_BUILD_NUMBER} "0" PARENT_SCOPE)
        endif()
    endif()
endfunction()

# If the optional argument BUILD_NUMBER is specified, tries to detect the build number of a Server
# compatible with the current build from git tags. The detection algorithm follows:
# 1. Let the "current" commit be the commit pointed to by the HEAD of the git repository.
# 2. Check if the "current" commit belongs to one of the "protected" branches (either "master",
#     "vms_<version>" or "vms_<version>_patch", where "version" has the format "<digit>.<digit>").
#     If it does, jump to step 4.
# 3. Take the parent of the "current" commit as a new "current" commit and go back to step 2.
# 4. If the "current" commit does not have excatly one tag of the form
#     "vms/<version>/<build_number>_...", set BUILD_NUMBER to 0 and exit.
# 5. Set BUILD_NUMBER to the value extracted from the tag.
# 6. Check that there are no more tags of the same form pointing to the same commit; otherwise,
#     issue a warning.
#
# If any `git` command returns an error, issue a warning and set BUILD_NUMBER to 0.
function(_extract_git_info)
    cmake_parse_arguments(GIT "" "CHANGESET;BRANCH;CURRENT_REFS;BUILD_NUMBER" "" ${ARGN})

    set(dir ${CMAKE_CURRENT_LIST_DIR})

    set(_changeset "")
    execute_process(
        COMMAND git -C "${dir}" rev-parse --short=12 HEAD
        OUTPUT_VARIABLE _changeset
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND git -C "${dir}" status --porcelain
        OUTPUT_VARIABLE is_dirty
        RESULT_VARIABLE is_status_failed
    )
    # If `git status` command is not successful, assign an empty string to the GIT_CHANGESET
    # variable in the parent scope and return from the function. An empty value will cause a fatal
    # error and stop the build process.
    if(NOT is_status_failed STREQUAL "0")
        set(${GIT_CHANGESET} "" PARENT_SCOPE)
        return()
    endif()
    # When `git status` command returns anything but empty string, it means that the working tree
    # is dirty.
    if(is_dirty)
        string(APPEND _changeset "+")
    endif()
    set(${GIT_CHANGESET} "${_changeset}" PARENT_SCOPE)

    set(_branch "")
    execute_process(
        COMMAND
            git -C "${dir}" branch --format "%(if)%(HEAD) %(then)%(refname:short) %(else) %(end)"
        OUTPUT_VARIABLE _branch
        RESULT_VARIABLE is_branch_failed
    )
    # If `git branch` command is not successful, assign an empty string to the GIT_BRANCH variable
    # in the parent scope and return from the function. An empty value will cause a fatal error and
    # stop the build process.
    if(NOT is_branch_failed STREQUAL "0")
        set(${GIT_BRANCH} "" PARENT_SCOPE)
        return()
    endif()
    string(STRIP "${_branch}" _branch)
    # If the result of the `git branch` command contains a space, then it is not a
    # valid branch name, hence the tree is in the "DETACHED HEAD" state.
    if(_branch MATCHES " ")
        set(_branch "DETACHED_HEAD")
    endif()
    set(${GIT_BRANCH} "${_branch}" PARENT_SCOPE)

    set(_current_refs_output "")
    execute_process(
        COMMAND git -C "${dir}" log --decorate -n1 --format=format:%D
        OUTPUT_VARIABLE _current_refs_output
        RESULT_VARIABLE is_log_failed
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # If `git log` command is not successful, assign an empty string to the GIT_CURRENT_REFS
    # variable in the parent scope and return from the function. An empty value will cause a fatal
    # error and stop the build process.
    if(NOT is_log_failed STREQUAL "0")
        set(${GIT_CURRENT_REFS} "" PARENT_SCOPE)
        return()
    endif()
    # If _current_refs_output is empty, GIT_CURRENT_REFS variable in the parent scope will have
    # the default value.
    if(_current_refs_output)
        string(REGEX REPLACE
            "HEAD, |HEAD -> " "" _current_refs_with_spaces "${_current_refs_output}")
        string(REGEX REPLACE " " "" _current_refs "${_current_refs_with_spaces}")
        set(${GIT_CURRENT_REFS} "${_current_refs}" PARENT_SCOPE)
    endif()

    if(GIT_BUILD_NUMBER)
        _detect_compatible_release_build_number(_buildNumber)
        set(${GIT_BUILD_NUMBER} ${_buildNumber} PARENT_SCOPE)
    endif()
endfunction()

function(_detect_compatible_release_build_number build_number_variable)
    set(fork_point "")
    _find_protected_branch_fork_point(fork_point)

    # This command finds zero or one tag. In the latter case it is the the most recent one.
    set(get_commit_release_tag_command
        git -C "${CMAKE_CURRENT_LIST_DIR}" describe --tags --match "${META_RELEASE_TAG_GLOB}"
            --exact-match "${fork_point}"
    )
    execute_process(
        COMMAND ${get_commit_release_tag_command}
        OUTPUT_VARIABLE _git_tag
        RESULT_VARIABLE _return_code
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_return_code EQUAL 0 AND _git_tag MATCHES ${META_RELEASE_TAG_REGEX})
        set(${build_number_variable} ${CMAKE_MATCH_2})
        # Check if there are other release tags pointing at the found commit and issue a warning
        # if there are any.
        _issue_multiple_tags_warning_if_needed(${_git_tag})
    else()
        set(${build_number_variable} "0")
    endif()

    nx_expose_to_parent_scope(${build_number_variable})
endfunction()

# If the current commit belongs to a "protected" branch, set fork_point_variable to "HEAD~0";
# otherwise, find the first ancestor of the current commit, which does, and set fork_point_variable
# to "HEAD~<number_of_commits_between_the_found_and_current_commits>".
#
# NOTE: A "Protected" branch here is a branch with the name "master", "vms_<version>" or
# "vms_<version>_patch", where "version" has a form "<digit>.<digit>".
function(_find_protected_branch_fork_point fork_point_variable)
    set(get_branch_names_command
        git -C "${CMAKE_CURRENT_LIST_DIR}" branch --format=%\(refname:lstrip=2\) --contains
    )

    set(${fork_point_variable} "")
    set(commit_index 0)
    while(NOT ${fork_point_variable})
        set(git_command ${get_branch_names_command} HEAD~${commit_index})
        _execute_git_command_and_return_if_error("${git_command}" RESULT_VARIABLE_NAME _branches)
        string(REPLACE "\n" ";" _branches_array "${_branches}")
        foreach(branch ${_branches_array})
            if(branch MATCHES ${PROTECTED_BRANCH_REGEX})
                set(${fork_point_variable} "HEAD~${commit_index}")
                break()
            endif()
        endforeach()

        math(EXPR commit_index "${commit_index} + 1")
    endwhile()

    nx_expose_to_parent_scope(${fork_point_variable})
endfunction()

function(_issue_multiple_tags_warning_if_needed primary_git_tag)
    set(get_all_git_tags_command
        git -C "${CMAKE_CURRENT_LIST_DIR}" tag --points-at "${primary_git_tag}"
    )
    _execute_git_command_and_return_if_error(
        "${get_all_git_tags_command}" RESULT_VARIABLE_NAME _git_tag_points_at_result)
    string(REPLACE "\n" ";" _git_tag_points_at_result_array "${_git_tag_points_at_result}")
    foreach(tag ${_git_tag_points_at_result_array})
        if(tag MATCHES ${META_RELEASE_TAG_REGEX} AND NOT tag STREQUAL ${primary_git_tag})
            message(WARNING
                "More than one release tag pointing to the same commit is found. Using the most "
                "recent one (${primary_git_tag}) to auto-detect build number."
            )
            break()
        endif()
    endforeach()
endfunction()

macro(_execute_git_command_and_return_if_error command)
    cmake_parse_arguments(ARGS "" "RESULT_VARIABLE_NAME" "" ${ARGN})
    execute_process(
        COMMAND ${command}
        OUTPUT_VARIABLE ${ARGS_RESULT_VARIABLE_NAME}
        RESULT_VARIABLE _return_code
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _return_code EQUAL 0)
        string(REPLACE ";" " " _command_string "${command}")
        message(WARNING
            "Command \"${_command_string}\" failed. Can be a sign of problems with the git "
            "repository.")
        return()
    endif()
endmacro()

# "buildNumber" cache variable must be set.
function(nx_vcs_get_meta_release_build_suffix suffix_variable)
    set(tag_glob "vms/[0-9].[0-9]*/${buildNumber}*_meta_*")
    set(get_commit_release_tag_command
        git -C "${CMAKE_CURRENT_LIST_DIR}" describe --tags --match "${tag_glob}"
    )
    execute_process(
        COMMAND ${get_commit_release_tag_command}
        OUTPUT_VARIABLE _git_tag
        RESULT_VARIABLE _return_code
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_return_code EQUAL 0 AND _git_tag MATCHES ${META_RELEASE_TAG_REGEX} AND CMAKE_MATCH_2)
        string(REPLACE "_" "-" _suffix_variable ${CMAKE_MATCH_3})
        set(${suffix_variable} ${_suffix_variable} PARENT_SCOPE)
    else()
        set(${suffix_variable} "" PARENT_SCOPE)
    endif()
endfunction()
