function(vcs_changeset dir var)
    if(EXISTS ${dir}/.git/)
        git_changeset(${dir} ${var})
    elseif(EXISTS ${dir}/.hg/)
        hg_changeset(${dir} ${var})
    else()
        message(WARNING "Can't get changeset: not found any VCS")
    endif()
    set(${var} ${${var}} PARENT_SCOPE)
endfunction()

function(vcs_branch dir var)
    if(EXISTS ${dir}/.git/)
        git_branch(${dir} ${var})
    elseif(EXISTS ${dir}/.hg/)
        hg_branch(${dir} ${var})
    else()
        message(WARNING "Can't get current branch: not found any VCS")
    endif()
    set(${var} ${${var}} PARENT_SCOPE)
endfunction()

function(hg_changeset dir var)
    execute_process(
        COMMAND hg --repository "${dir}" id -i
        OUTPUT_VARIABLE changeset
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REPLACE "+" "" changeset "${changeset}")
    set(${var} ${changeset} PARENT_SCOPE)
endfunction()

function(hg_branch dir var)
    execute_process(
        COMMAND hg --repository "${dir}" branch
        OUTPUT_VARIABLE branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${var} "${branch}" PARENT_SCOPE)
endfunction()

function(git_changeset dir var)
    execute_process(
        COMMAND git -C "${dir}" rev-parse HEAD
        OUTPUT_VARIABLE changeset
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${var} ${changeset} PARENT_SCOPE)
endfunction()

function(git_branch dir var)
    execute_process(
        COMMAND git -C "${dir}" rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${var} "${branch}" PARENT_SCOPE)
endfunction()

#function(hg_last_commit_branch dir var)
#    execute_process(
#        COMMAND hg --repository "${dir}" log --template "{branch}" -r -1
#        OUTPUT_VARIABLE parent_branch
#        OUTPUT_STRIP_TRAILING_WHITESPACE
#    )
#    set(${var} "${parent_branch}" PARENT_SCOPE)
#endfunction()

#function(hg_parent_branch dir branch var)
#    execute_process(
#        COMMAND hg --repository "${dir}"
#            log --template "{branch}" -r "parents(min(branch(${branch})))"
#        OUTPUT_VARIABLE parent_branch
#        OUTPUT_STRIP_TRAILING_WHITESPACE
#    )
#    set(${var} "${parent_branch}" PARENT_SCOPE)
#endfunction()
