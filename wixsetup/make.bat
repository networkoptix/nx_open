set "CURRENTDIR=%cd%"
cd..
call convert.bat

call "%VS90COMNTOOLS%vsvars32.bat"
call qtvars.bat

cd %CURRENTDIR%

MSBuild ..\src\uniclient.vcproj /t:Rebuild /p:Configuration=Release
MSBuild PropsCA\PropsCA.vcproj /t:Rebuild /p:Configuration=Release
MSBuild EveAssocCA\EveAssocCA.vcproj /t:Rebuild /p:Configuration=Release

@start /B /WAIT make.py
