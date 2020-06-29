call current_branch.bat
crowdin upload translations -b %CURRENT_BRANCH% --config crowdin-vms-base.yaml --no-auto-approve-imported --no-import-eq-suggestions %*
