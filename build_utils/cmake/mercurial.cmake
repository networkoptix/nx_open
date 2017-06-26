function(hg_changeset dir var)
    execute_process(
        COMMAND hg --repository "${dir}" id -i
        OUTPUT_VARIABLE changeSet
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${var} ${changeSet} PARENT_SCOPE)
endfunction()

function(hg_branch dir var)
    execute_process(
        COMMAND hg --repository "${dir}" branch
        OUTPUT_VARIABLE branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${var} ${branch} PARENT_SCOPE)
endfunction()

function(hg_last_commit_branch dir var)
    execute_process(
        COMMAND hg --repository "${dir}" log --template "{branch}" -r -1
        OUTPUT_VARIABLE parent_branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${var} ${parent_branch} PARENT_SCOPE)
endfunction()

function(hg_parent_branch dir branch var)
    execute_process(
        COMMAND hg --repository "${dir}"
            log --template "{branch}" -r "parents(min(branch(${branch})))"
        OUTPUT_VARIABLE parent_branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${var} ${parent_branch} PARENT_SCOPE)
endfunction()
