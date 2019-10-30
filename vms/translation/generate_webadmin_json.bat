set CURRENTDIR=%cd%
cd ..\..\webadmin
python generate_language_json.py
cd %CURRENTDIR%
