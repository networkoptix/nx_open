@echo off

set CUSTOMIZATION=
set ARCH=
set BUILDNUMBER=-DbuildNumber=4814

set INPUT=%1 
echo customization=%INPUT%
if not [%1] == [] set CUSTOMIZATION=-Dcustomization=%INPUT%

set INPUT_ARCH=%2
if not [%2] == [] set ARCH=-Darch=%INPUT_ARCH%
echo ARCH=%INPUT_ARCH%

@echo on
call mvn package -T 4 --projects build_environment %CUSTOMIZATION% %ARCH% %BUILDNUMBER%
call mvn package -T 4 --projects appserver,genericrtspplugin,axiscamplugin -P!installer %CUSTOMIZATION% %ARCH% %BUILDNUMBER%
call mvn compile -T 4 -rf common -P!installer %CUSTOMIZATION% %ARCH% %BUILDNUMBER%
call mvn exec:exec -T 4 --projects common,client,vmaxproxy %CUSTOMIZATION% %ARCH% %BUILDNUMBER%
call mvn exec:exec -T 4 --projects mediaserver,mediaproxy,quicksyncdecoder,traytool,applauncher %CUSTOMIZATION% %ARCH% %BUILDNUMBER%
call mvn package -T 4 --projects wixsetup\wixsetup-full,wixsetup\wixsetup-client-only %CUSTOMIZATION% %ARCH% %BUILDNUMBER%