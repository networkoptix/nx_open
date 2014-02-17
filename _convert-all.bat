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
call mvn compile --projects build_environment %CUSTOMIZATION% %ARCH%
call mvn compile --projects build_variables %CUSTOMIZATION% %ARCH%
call mvn compile -T 4 -rf common %CUSTOMIZATION% %ARCH%