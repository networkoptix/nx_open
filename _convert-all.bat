@echo off
set INPUT=%1 

set CUSTOMIZATION=

echo customization=%INPUT%
if not [%1] == [] set CUSTOMIZATION=-Dcustomization=%INPUT%

@echo on
call mvn compile -T 4 --projects build-environment %CUSTOMIZATION%
call mvn compile -T 4 -rf appserver -P!installer %CUSTOMIZATION%