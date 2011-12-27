call "%VS90COMNTOOLS%vsvars32.bat"
call qtvars.bat

set "CURRENTDIR=%cd%"

cd ..\common
rem @start /B /WAIT convert.py

cd ..\client
rem @start /B /WAIT convert.py

cd ..\server
rem @start /B /WAIT convert.py

cd ..\appserver
rem @start /B /WAIT convert.py
rem @start /B /WAIT setup.py build

cd %CURRENTDIR%

rem MSBuild ..\common\src\common.vcproj /t:Rebuild /p:Configuration=Release
rem MSBuild ..\client\src\client.vcproj /t:Rebuild /p:Configuration=Release
rem MSBuild ..\server\src\server.vcproj /t:Rebuild /p:Configuration=Release

MSBuild PropsCA\PropsCA.vcproj /t:Rebuild /p:Configuration=Release
MSBuild EveAssocCA\EveAssocCA.vcproj /t:Rebuild /p:Configuration=Release

@start /B /WAIT make.py
