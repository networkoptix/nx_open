@echo off

call test.py
call version.bat
SET VERSION=%APPLICATION_VERSION%.%BUILD_NUMBER%
SET "CURRENTDIR=%cd%"
SET ARTIFACTID=HDWitness

echo version = %VERSION%

rmdir /s /q "c:\records"

CD %CURRENTDIR%

start /B /WAIT msiexec /I "bin\%ARTIFACTID%-%VERSION%.msi" /qb- /Lv* "install.log" APPSERVER_PASSWORD=admin SERVER_HOST="localhost" SERVER_PORT="7001" SERVER_LOGIN="admin" SERVER_PASSWORD="admin" CLIENT_APPSERVER_HOST="localhost" CLIENT_APPSERVER_PORT="7001" CLIENT_APPSERVER_LOGIN="admin" CLIENT_APPSERVER_PASSWORD="admin" SERVER_DIRECTORY="c:\records"
echo installer errorlevel is %ERRORLEVEL%

cd "%VS90COMNTOOLS%\..\..\..\Network Optix\HD Witness\Client\"

rem ugly hack! --auth parameter is applied only in second launch!
call client.exe --test-timeout 5000 --auth "http://admin:admin@127.0.0.1:7001" --test-resource-substring Server

call client.exe --test-timeout 180000 --auth "http://admin:admin@127.0.0.1:7001" --test-resource-substring Server > test.txt
SET TESTRESULT=%ERRORLEVEL%
echo test errorlevel is %TESTRESULT%

sc stop vmsmediaserver
sc stop vmsappserver
taskkill /F /IM traytool.exe
taskkill /F /IM client.exe
taskkill /F /IM mediaserver.exe
taskkill /F /IM ec.exe

CD %CURRENTDIR%

call msiexec /x "bin\%ARTIFACTID%-%VERSION%.msi" /qn /l*v uninstall.log
echo uninstaller errorlevel is %ERRORLEVEL%

rmdir /s /q "%VS90COMNTOOLS%\..\..\..\Network Optix"

if "%TESTRESULT%" == "1" exit /B 1
