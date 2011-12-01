set "CURRENTDIR=%cd%"
set "CONFIG=debug"
rem call mvn
cd %CURRENTDIR%\appserver
@start runserver.bat
cd %CURRENTDIR%\server\bin\%CONFIG%\
@start server.exe
cd %CURRENTDIR%\client\bin\%CONFIG%\
@start /B client.exe
cd %CURRENTDIR%