@echo off
crowdin download -b cloud --config crowdin-cloud.yaml
FOR /F "delims==" %%G IN ('dir /B ..\webadmin\translations') DO robocopy "..\webadmin\translations\%%G\web_common" "..\cloud_portal\translations\%%G\web_common" /s