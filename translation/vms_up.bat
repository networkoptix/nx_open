set CURRENTDIR=%cd%
cd ..\webadmin
python generate_language_json.py
cd %CURRENTDIR%
crowdin upload sources -b default --config crowdin-vms.yaml
