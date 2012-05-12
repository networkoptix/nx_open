@echo off
set INPUT=%1 

set CUSTOMIZATION=

echo customization=%INPUT%
if not [%1] == [] set CUSTOMIZATION=-Dcustomization=%INPUT%

@echo on

SET CURRENTDIR=%cd%

cd ..\common
call mvn compile -P!convert-clean-windows,!convert-clean %CUSTOMIZATION% -DBUILD_NUMBER=0

cd %CURRENTDIR%
call mvn compile -P!convert-clean-windows,!convert-clean %CUSTOMIZATION% -DBUILD_NUMBER=0