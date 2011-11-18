call "%VS90COMNTOOLS%vsvars32.bat"
call qtvars.bat

set "CURRENTDIR=%cd%"

cd ..\common
@start /B /WAIT convert.py

cd ..\client
@start /B /WAIT convert.py

cd ..\server
@start /B /WAIT convert.py

cd ..\appserver
@start /B /WAIT convert.py

cd %CURRENTDIR%

MSBuild ..\common\src\common.vcproj /t:Rebuild /p:Configuration=Release
MSBuild ..\client\src\client.vcproj /t:Rebuild /p:Configuration=Release
MSBuild ..\server\src\server.vcproj /t:Rebuild /p:Configuration=Release

rem MSBuild PropsCA\PropsCA.vcproj /t:Rebuild /p:Configuration=Release
rem MSBuild EveAssocCA\EveAssocCA.vcproj /t:Rebuild /p:Configuration=Release

rem @start /B /WAIT make.py
