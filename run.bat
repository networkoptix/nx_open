set "CURRENTDIR=%cd%"
set "CONFIG=debug"
rem call mvn
cd %CURRENTDIR%\appserver
@start runserver.bat
cd %CURRENTDIR%\mediaserver\bin\%CONFIG%\
@start mediaserver.exe -s
cd %CURRENTDIR%\client\bin\%CONFIG%\
@start /B client.exe
cd %CURRENTDIR%
