call current_branch.bat
crowdin upload translations -l pt-PT --no-auto-approve-imported --no-import-eq-suggestions -b %CURRENT_BRANCH% --config crowdin-vms-base.yaml %*
