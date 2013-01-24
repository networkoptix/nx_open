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
call mvn package -T 4 --projects build-environment,appserver -P!installer %CUSTOMIZATION% %ARCH% 
call mvn compile -T 4 -rf common -P!installer %CUSTOMIZATION% %ARCH% 
call mvn exec:exec -T 4 --projects common,client %CUSTOMIZATION% %ARCH% 
call mvn exec:exec -T 4 --projects mediaserver,mediaproxy,quicksyncdecoder,traytool %CUSTOMIZATION% %ARCH% 
call mvn package -T 4 --projects wixsetup %CUSTOMIZATION% %ARCH%