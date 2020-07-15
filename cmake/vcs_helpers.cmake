include(CMakeParseArguments)

function(nx_vcs_get_info)
    cmake_parse_arguments(GIT "" "CHANGESET;BRANCH;CURRENT_REFS" "" ${ARGN})

    set(_changeset "")
    set(_branch "")
    set(_current_refs "")

    if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/git_info.txt)
        message("ATTENTION: Using git information from ${CMAKE_CURRENT_LIST_DIR}/git_info.txt.")
        _read_git_info(CHANGESET _changeset BRANCH _branch CURRENT_REFS _current_refs)
    elseif(EXISTS ${CMAKE_CURRENT_LIST_DIR}/.git)
        _extract_git_info(CHANGESET _changeset BRANCH _branch CURRENT_REFS _current_refs)
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

    set(dir ${CMAKE_CURRENT_LIST_DIR}/.git)

    set(_changeset "")
    execute_process(
        COMMAND git -C "${dir}" rev-parse --short=12 HEAD
        OUTPUT_VARIABLE _changeset
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND git -C "${dir}" diff-index --quiet HEAD
        RESULT_VARIABLE is_dirty
    )
    if(is_dirty STREQUAL "1")
        string(APPEND _changeset "+")
    endif()
    set(${GIT_CHANGESET} "${_changeset}" PARENT_SCOPE)

    set(_branch "")
    execute_process(
        COMMAND git -C "${dir}" symbolic-ref --short HEAD
        OUTPUT_VARIABLE _branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _branch)
        set(_branch "DETACHED_HEAD")
    endif()
    set(${GIT_BRANCH} "${_branch}" PARENT_SCOPE)

    set(_current_refs_output "")
    execute_process(
        COMMAND git -C "${dir}" log --decorate -n1 --format=format:"%D"
        OUTPUT_VARIABLE _current_refs_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # if _current_refs_output is empty, _current_refs will be empty too.
    string(REGEX REPLACE "HEAD, |HEAD -> " "" _current_refs "${_current_refs_output}")
    set(${GIT_CURRENT_REFS} "${_current_refs}" PARENT_SCOPE)
endfunction()
