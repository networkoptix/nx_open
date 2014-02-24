@echo off

set CUSTOMIZATION=
set ARCH=
set BUILDNUMBER=-DbuildNumber=10000

set INPUT=%1 
echo customization=%INPUT%
if not [%1] == [] set CUSTOMIZATION=-Dcustomization=%INPUT%

set INPUT_ARCH=%2
if not [%2] == [] set ARCH=-Darch=%INPUT_ARCH%
echo ARCH=%INPUT_ARCH%

@echo on
call hg purge --all
call mvn compile -T 4 --projects build_environment,build_variables %CUSTOMIZATION% %ARCH% %BUILDNUMBER%
call mvn package -T 5 --projects common,client,appserver,plugins/genericrtspplugin,plugins/image_library_plugin,vmaxproxy %CUSTOMIZATION% %ARCH% %BUILDNUMBER%
call mvn package -T 6 --projects mediaserver,mediaproxy,applauncher,plugins/mjpg_link,traytool %CUSTOMIZATION% %ARCH% %BUILDNUMBER%
call mvn package -T 4 -rf wixsetup\wixsetup-full %CUSTOMIZATION% %ARCH% %BUILDNUMBER%