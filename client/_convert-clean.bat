@echo off
set INPUT=%1 

set CUSTOMIZATION=

echo customization=%INPUT%
if not [%1] == [] set CUSTOMIZATION=-Dcustomization=%INPUT%

@echo on
mvn clean compile %CUSTOMIZATION% -DBUILD_NUMBER=0