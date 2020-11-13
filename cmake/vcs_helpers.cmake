include(CMakeParseArguments)

function(nx_vcs_get_info)
    cmake_parse_arguments(GIT "" "CHANGESET;BRANCH;CURRENT_REFS" "" ${ARGN})

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

    if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/git_info.txt)
        message("ATTENTION: Using git information from ${CMAKE_CURRENT_LIST_DIR}/git_info.txt.")
        _read_git_info(CHANGESET _changeset BRANCH _branch CURRENT_REFS _current_refs)
    elseif(EXISTS ${CMAKE_CURRENT_LIST_DIR}/.git)
        _extract_git_info(CHANGESET _changeset BRANCH _branch CURRENT_REFS _current_refs)
    else()
        message(
            "ATTENTION: Neither git_info.txt nor .git subdirectory is found in "
            "${CMAKE_CURRENT_LIST_DIR}.")
    endif()

    set(${GIT_CHANGESET} ${_changeset} PARENT_SCOPE)
    set(${GIT_BRANCH} ${_branch} PARENT_SCOPE)
    set(${GIT_CURRENT_REFS} ${_current_refs} PARENT_SCOPE)
endfunction()

# Git info file inherits the format from build_info.txt, but contains only info obtained from git,
# i.e. parameters `changeSet=`, `branch=`, and `currentRefs=`, in this particular order. It is
# expected to be generated externally when building without the git repo available (e.g. on a
# remote host).
function(_read_git_info)
    cmake_parse_arguments(GIT "" "CHANGESET;BRANCH;CURRENT_REFS" "" ${ARGN})

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
endfunction()

function(_extract_git_info)
    cmake_parse_arguments(GIT "" "CHANGESET;BRANCH;CURRENT_REFS" "" ${ARGN})

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
endfunction()
