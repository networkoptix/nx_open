set CURRENTDIR=%cd%
cd ..\webadmin
python generate_language_json.py
cd %CURRENTDIR%
crowdin upload sources -b vms_4.0 --config crowdin-vms.yaml
