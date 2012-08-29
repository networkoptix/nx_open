call "%VS90COMNTOOLS%vsvars32.bat"
call qtvars.bat

set "CURRENTDIR=%cd%"

cd %~dp0

set CONFIG=%1

if [%1] == [] set CONFIG=Debug

MSBuild common\src\common-x86.vcproj /t:Build /p:Configuration=%CONFIG%
MSBuild client\src\client-x86.vcproj /t:Build /p:Configuration=%CONFIG%
MSBuild mediaserver\src\mediaserver-x86.vcproj /t:Build /p:Configuration=%CONFIG%
MSBuild mediaproxy\src\mediaproxy-x86.vcproj /t:Build /p:Configuration=%CONFIG%
MSBuild traytool\src\traytool-x86.vcproj /t:Build /p:Configuration=%CONFIG%

cd %CURRENTDIR%
