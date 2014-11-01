@echo off

SET EC_HOST=localhost
SET EC_PORT=7001
SET EC_LOGIN=admin
SET EC_PASSWORD=123
SET PROXY_PORT=7301
SET SERVER_DIRECTORY=c:\records
SET SERVER_RTSP_PORT=7101
SET SERVER_API_PORT=7201

SET VERSION=${release.version}.${buildNumber}
SET "CURRENTDIR=${project.build.directory}"
SET ARTIFACTID=${project.parent.artifactId}

echo version = %VERSION%

rmdir /s /q "%SERVER_DIRECTORY%"

CD %CURRENTDIR%

start /B /WAIT msiexec /I "bin\${finalName}.msi" /qb- /Lv* "install.log" APPSERVER_PASSWORD="%EC_PASSWORD%" APPSERVER_PORT="%EC_PORT%" PROXY_PORT="%PROXY_PORT%" SERVER_APPSERVER_HOST="%EC_HOST%" SERVER_APPSERVER_PORT="%EC_PORT%" SERVER_APPSERVER_LOGIN="%EC_LOGIN%" SERVER_APPSERVER_PASSWORD="%EC_PASSWORD%" SERVER_RTSP_PORT="%SERVER_RTSP_PORT%" SERVER_API_PORT="%SERVER_API_PORT%" CLIENT_APPSERVER_HOST="%EC_HOST%" CLIENT_APPSERVER_PORT="%EC_PORT%" CLIENT_APPSERVER_LOGIN="%EC_LOGIN%" CLIENT_APPSERVER_PASSWORD="%EC_PASSWORD%" SERVER_DIRECTORY="%SERVER_DIRECTORY%"
echo installer errorlevel is %ERRORLEVEL%

cd "%VS90COMNTOOLS%\..\..\..\${company.name}\${product.name}\Client\"

rem ugly hack! --auth parameter is applied only in second launch!
rem call client.exe --test-timeout 5000 --auth "https://%EC_LOGIN%:%EC_PASSWORD%@%EC_HOST%:%EC_PORT%" --test-resource-substring Server

call hdwitness.exe --test-timeout 180000 --auth "https://%EC_LOGIN%:%EC_PASSWORD%@%EC_HOST%:%EC_PORT%" --test-resource-substring Server > test.txt
SET TESTRESULT=%ERRORLEVEL%
echo test errorlevel is %TESTRESULT%

sc stop vmsmediaserver
sc stop vmsappserver

taskkill /F /IM traytool.exe
taskkill /F /IM hdwitness.exe
taskkill /F /IM mediaserver.exe
taskkill /F /IM ecs.exe

sc delete vmsappserver
sc delete vmsmediaserver

CD %CURRENTDIR%

call msiexec /x "bin\${finalName}.msi" /qn /l*v uninstall.log
echo uninstaller errorlevel is %ERRORLEVEL%

rmdir /s /q "%VS90COMNTOOLS%\..\..\..\Network Optix"

if "%TESTRESULT%" == "1" exit /B 1
