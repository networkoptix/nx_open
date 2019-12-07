@echo OFF
setlocal enableDelayedExpansion
FOR %%G IN (de, en-GB, en-US, es-ES, fr, he, hu, ja, ko, nl, pl, ru, th, tr, it, uk, vi, zh-CN, zh-TW) DO (
    crowdin download -b cloud_18.3 --config crowdin-cloud.yaml -l %%G
)
FOR /F "delims==" %%G IN ('dir /B ..\webadmin\translations') DO robocopy "..\webadmin\translations\%%G\web_common" "..\cloud_portal\translations\%%G\web_common" /s