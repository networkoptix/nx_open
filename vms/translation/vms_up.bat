call current_branch.bat
set CURRENTDIR=%cd%
cd ..\..\webadmin
python generate_language_json.py
cd %CURRENTDIR%
crowdin upload sources -b %CURRENT_BRANCH% --config crowdin-vms.yaml
