set CURRENTDIR=%cd%
cd ..\webadmin
python generate_language_json.py
cd %CURRENTDIR%
crowdin upload sources -b vms_3.2_hotfix --config crowdin-vms.yaml
