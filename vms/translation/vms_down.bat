call current_branch.bat
crowdin download -b %CURRENT_BRANCH% --config vms.yaml --debug %*
