call "%VS90COMNTOOLS%vsvars32.bat"
call qtvars.bat

call ..\make.bat %CONFIG%

MSBuild PropsCA\PropsCA.vcproj /t:Rebuild /p:Configuration=Release
MSBuild EveAssocCA\EveAssocCA.vcproj /t:Rebuild /p:Configuration=Release

@start /B /WAIT make.py %CONFIG%
