function(nx_vcs_changeset dir var)
    set(_changeset "")
    set(_reason "")

    if(EXISTS ${dir}/.git)
        _git_changeset(${dir} _changeset)
    else()
        set(_reason "VCS not detected")
    endif()

    if(_changeset STREQUAL "")
        set(_reason "Failed quering repository")

        set(_changeset_file ${dir}/changeset.txt)
        if(_changeset STREQUAL "" AND EXISTS ${_changeset_file})
            set(${var} "")
            file(READ ${_changeset_file} _changeset) #< On error, _changeset remains empty.
            string(STRIP ${_changeset} _changeset)
            if(_changeset STREQUAL "")
                set(_reason "Failed to read ${_changeset_file}")
            else()
                message(STATUS "ATTENTION: Using changeset from ${_changeset_file}: ${_changeset}")
            endif()
        endif()
    endif()

    if(_changeset STREQUAL "")
        message(WARNING "Can't get changeset: ${_reason}; settings ${var} to an empty string.")
    endif()

    set(${var} ${_changeset} PARENT_SCOPE)
endfunction()

function(nx_vcs_branch dir var)
    if(EXISTS ${dir}/.git)
        _git_branch(${dir} _branch)
    else()
        message(WARNING "Can't get current branch: VCS not detected: ${dir}")
    endif()
    set(${var} "${_branch}" PARENT_SCOPE)
endfunction()

function(nx_vcs_current_refs dir var)
    if(EXISTS ${dir}/.git)
        _git_current_refs(${dir} _current_refs)
    else()
        message(WARNING "Can't get current refs: VCS not detected: ${dir}")
    endif()
    set(${var} "${_current_refs}" PARENT_SCOPE)
endfunction()

function(_git_changeset dir var)
    execute_process(
        COMMAND git -C "${dir}" rev-parse --short=12 HEAD
        OUTPUT_VARIABLE changeset
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${var} "${changeset}" PARENT_SCOPE)
endfunction()

function(_git_branch dir var)
    execute_process(
        COMMAND git -C "${dir}" symbolic-ref --short HEAD
        OUTPUT_VARIABLE branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT branch)
        set(branch "DETACHED_HEAD")
    endif()
    set(${var} "${branch}" PARENT_SCOPE)
endfunction()

function(_git_current_refs dir var)
    execute_process(
        COMMAND git -C "${dir}" log --decorate -n1 --format=format:"%D"
        OUTPUT_VARIABLE current_refs_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX REPLACE "HEAD, |HEAD -> " "" current_refs "${current_refs_output}")
    set(${var} "${current_refs}" PARENT_SCOPE)
endfunction()
