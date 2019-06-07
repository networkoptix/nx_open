function(nx_vcs_changeset dir var)
    if(EXISTS ${dir}/.hg/)
        _hg_changeset(${dir} ${var})
    elseif(EXISTS ${dir}/.git/)
        _git_changeset(${dir} ${var})
    else()
        message(WARNING "Can't get changeset: not found any VCS")
    endif()
    set(${var} ${${var}} PARENT_SCOPE)
endfunction()

function(nx_vcs_branch dir var)
    if(EXISTS ${dir}/.hg/)
        _hg_branch(${dir} ${var})
    elseif(EXISTS ${dir}/.git/)
        _git_branch(${dir} ${var})
    else()
        message(WARNING "Can't get current branch: not found any VCS")
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
        COMMAND git -C "${dir}" rev-parse HEAD
        OUTPUT_VARIABLE changeset
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${var} ${changeset} PARENT_SCOPE)
endfunction()

function(_git_branch dir var)
    execute_process(
        COMMAND git -C "${dir}" rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${var} "${branch}" PARENT_SCOPE)
endfunction()
