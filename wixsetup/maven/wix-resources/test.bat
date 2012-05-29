@echo off

SET VERSION=${project.version}.${buildNumber}
SET "CURRENTDIR=${project.build.directory}"
SET ARTIFACTID=${project.parent.artifactId}

echo version = %VERSION%

rmdir /s /q "c:\records"

CD %CURRENTDIR%

start /B /WAIT msiexec /I "bin\${project.build.finalName}.msi" /qb- /Lv* "install.log" APPSERVER_PASSWORD=123 APPSERVER_PORT="7001" SERVER_APPSERVER_HOST="localhost" SERVER_APPSERVER_PORT="7001" SERVER_APPSERVER_LOGIN="admin" SERVER_APPSERVER_PASSWORD="123" CLIENT_APPSERVER_HOST="localhost" CLIENT_APPSERVER_PORT="7001" CLIENT_APPSERVER_LOGIN="admin" CLIENT_APPSERVER_PASSWORD="123" SERVER_DIRECTORY="c:\records"
echo installer errorlevel is %ERRORLEVEL%

cd "c:\Program Files\Network Optix\HD Witness\Client\"

rem ugly hack! --auth parameter is applied only in second launch!
call client.exe --test-timeout 5000 --auth "http://admin:admin@127.0.0.1:8000" --test-resource-substring Server

call client.exe --test-timeout 180000 --auth "http://admin:admin@127.0.0.1:8000" --test-resource-substring Server > test.txt
SET TESTRESULT=%ERRORLEVEL%
echo test errorlevel is %TESTRESULT%

sc stop vmsmediaserver
sc stop vmsappserver

taskkill /F /IM traytool.exe
taskkill /F /IM client.exe
taskkill /F /IM mediaserver.exe
taskkill /F /IM ec.exe

sc delete vmsappserver
sc delete vmsmediaserver

CD %CURRENTDIR%

call msiexec /x "bin\%ARTIFACTID%-%VERSION%.msi" /qn /l*v uninstall.log
echo uninstaller errorlevel is %ERRORLEVEL%

rmdir /s /q "%VS90COMNTOOLS%\..\..\..\Network Optix"

if "%TESTRESULT%" == "1" exit /B 1
