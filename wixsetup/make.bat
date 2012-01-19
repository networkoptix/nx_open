call "%VS90COMNTOOLS%vsvars32.bat"
call qtvars.bat

set "CURRENTDIR=%cd%"

cd ..\common
@start /B /WAIT convert.py

cd ..\client
@start /B /WAIT convert.py

cd ..\server
@start /B /WAIT convert.py

cd ..\appserver\setup
@start /B /WAIT setup.py build

cd %CURRENTDIR%

set CONFIG=%1

if [%1] == [] set CONFIG=Release

MSBuild ..\common\src\common.vcproj /t:Rebuild /p:Configuration=%CONFIG%
MSBuild ..\client\src\client.vcproj /t:Rebuild /p:Configuration=%CONFIG%
MSBuild ..\server\src\server.vcproj /t:Rebuild /p:Configuration=%CONFIG%

MSBuild PropsCA\PropsCA.vcproj /t:Rebuild /p:Configuration=Release
MSBuild EveAssocCA\EveAssocCA.vcproj /t:Rebuild /p:Configuration=Release

@start /B /WAIT make.py %CONFIG%
