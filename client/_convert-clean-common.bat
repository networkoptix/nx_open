@echo off
set INPUT=%1 

set CUSTOMIZATION=

echo customization=%INPUT%
if not [%1] == [] set CUSTOMIZATION=-Dcustomization=%INPUT%

@echo on

SET CURRENTDIR=%cd%

cd ..\common
call mvn clean compile %CUSTOMIZATION%

cd %CURRENTDIR%
call mvn clean compile %CUSTOMIZATION%