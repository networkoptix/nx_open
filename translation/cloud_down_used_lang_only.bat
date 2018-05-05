@echo OFF
setlocal enableDelayedExpansion
FOR %%G IN (de, en-GB, en-US, es-ES, fr, he, hu, ja, ko, nl, pl, ru, th, tr, vi, zh-CN, zh-TW) DO (
    crowdin download -b cloud --config crowdin-cloud.yaml -l %%G
)