call "%VS90COMNTOOLS%vsvars32.bat"
call qtvars.bat

set "CURRENTDIR=%cd%"

cd %~dp0

SET QT_PATH=c:\develop\Qt\4.7.4

SET PATH=%PATH%;%QT_PATH%\bin

rem call convert.bat

set CONFIG=%1

if [%1] == [] set CONFIG=Debug

MSBuild common\src\common.vcproj /t:Build /p:Configuration=%CONFIG%
MSBuild client\src\client.vcproj /t:Build /p:Configuration=%CONFIG%
MSBuild mediaserver\src\server.vcproj /t:Build /p:Configuration=%CONFIG%
MSBuild mediaproxy\src\mediaproxy.vcproj /t:Build /p:Configuration=%CONFIG%
MSBuild traytool\src\traytool.vcproj /t:Build /p:Configuration=%CONFIG%

cd %CURRENTDIR%
