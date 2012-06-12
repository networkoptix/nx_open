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
mvn clean install %CUSTOMIZATION% %ARCH% -Dmaven.test.skip=true