SET VERSION=1.2.91

del /F /P c:\records
start /B /WAIT msiexec /I "bin\VMS-%VERSION%.msi" /qb- /Lv* "install.log" SERVER_HOST="localhost" SERVER_PORT="8000" SERVER_LOGIN="appserver" SERVER_PASSWORD="123" CLIENT_APPSERVER_HOST="localhost" CLIENT_APPSERVER_PORT="8000" CLIENT_APPSERVER_LOGIN="appserver" CLIENT_APPSERVER_PASSWORD="123" SERVER_DIRECTORY="c:\records"
"%VS90COMNTOOLS%\..\..\..\Network Optix\VMS\Client\client.exe" --test-timeout 15000 --test-resource-substring Server > test.txt
call msiexec /x "bin\VMS-%VERSION%.msi" /qn /l*v uninstall.log