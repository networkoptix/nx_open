@echo off
crowdin download -b cloud_18.3 --config crowdin-cloud.yaml %*
FOR /F "delims==" %%G IN ('dir /B ..\webadmin\translations') DO robocopy "..\webadmin\translations\%%G\web_common" "..\cloud_portal\translations\%%G\web_common" /s