call "%VS90COMNTOOLS%vsvars32.bat"
call qtvars.bat

set "CURRENTDIR=%cd%"

cd %~dp0

cd common
@start /B /WAIT convert.py
cd %~dp0

cd client
@start /B /WAIT convert.py
cd %~dp0

cd server
@start /B /WAIT convert.py
cd %~dp0

set CONFIG=%1

if [%1] == [] set CONFIG=Debug

MSBuild common\src\common.vcproj /t:Rebuild /p:Configuration=%CONFIG%
MSBuild client\src\client.vcproj /t:Rebuild /p:Configuration=%CONFIG%
MSBuild server\src\server.vcproj /t:Rebuild /p:Configuration=%CONFIG%

cd %CURRENTDIR%
