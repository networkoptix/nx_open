set CURRENTDIR=%cd%
cd ..\webadmin
python generate_language_json.py
cd %CURRENTDIR%
crowdin upload sources -b vms_3.1 --config crowdin-vms.yaml
