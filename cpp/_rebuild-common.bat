@echo off

set CUSTOMIZATION=
set ARCH=

set INPUT=%1 
echo customization=%INPUT%
if not [%1] == [] set CUSTOMIZATION=-Dcustomization=%INPUT%

set INPUT_ARCH=%2
if not [%2] == [] set ARCH=-Darch=%INPUT_ARCH%
echo ARCH=%INPUT_ARCH%

@echo on

SET CURRENTDIR=%cd%

cd ..\common
start /B cmd.exe /K call mvn package -U %CUSTOMIZATION% %ARCH%

cd %CURRENTDIR%
start /B cmd.exe /K call mvn package -U %CUSTOMIZATION% %ARCH%