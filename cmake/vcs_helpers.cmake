function(nx_vcs_changeset dir var)
    # hg yields such changeset if .hg dir exists but hg does not detect a repo there.
    set(_unknown_changeset "000000000000")

    set(${var} ${unknown_changeset})
    set(_reason "")

    if(EXISTS ${dir}/.hg/)
        _hg_changeset(${dir} ${var})
    elseif(EXISTS ${dir}/.git)
        _git_changeset(${dir} ${var})
    else()
        set(_reason "VCS not detected")
    endif()
    
    if(_reason STREQUAL "")
        set(_reason "Failed quering repository")
    endif()
    
    set(_changeset_file ${dir}/changeset.txt)
    if(${var} STREQUAL ${_unknown_changeset} AND EXISTS ${_changeset_file})
        file(READ ${_changeset_file} ${var}) #< On error, ${var} retains _unknown_changeset.
        if(${var} STREQUAL ${_unknown_changeset})
                set(_reason "Failed to read ${_changeset_file}")
        else()
            message(STATUS "ATTENTION: Using changeset from ${_changeset_file}: ${${var}}")
        endif()
    endif()
    
    if(${var} STREQUAL ${_unknown_changeset})
        message(WARNING "Can't get changeset: ${_reason}; assuming ${${var}}.")
    endif()
    
    set(${var} ${${var}} PARENT_SCOPE)
endfunction()

function(nx_vcs_branch dir var)
    if(EXISTS ${dir}/.hg/)
        _hg_branch(${dir} ${var})
    elseif(EXISTS ${dir}/.git)
        _git_branch(${dir} ${var})
    else()
        message(WARNING "Can't get current branch: not found any VCS: ${dir}")
    endif()
    set(${var} ${${var}} PARENT_SCOPE)
endfunction()

function(nx_vcs_current_refs dir var)
    if(EXISTS ${dir}/.git)
        _git_current_refs(${dir} ${var})
    else()
        message(WARNING "Can't get current refs: not found any VCS: ${dir}")
    endif()
    set(${var} ${${var}} PARENT_SCOPE)
endfunction()

function(_hg_changeset dir var)
    execute_process(
        COMMAND hg --repository "${dir}" log --rev . --template "{node|short}"
        OUTPUT_VARIABLE changeset
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${var} ${changeset} PARENT_SCOPE)
endfunction()

function(_hg_branch dir var)
    execute_process(
        COMMAND hg --repository "${dir}" branch
        OUTPUT_VARIABLE branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${var} "${branch}" PARENT_SCOPE)
endfunction()

function(_git_changeset dir var)
    execute_process(
        COMMAND git -C "${dir}" rev-parse --short=12 HEAD
        OUTPUT_VARIABLE changeset
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${var} ${changeset} PARENT_SCOPE)
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
