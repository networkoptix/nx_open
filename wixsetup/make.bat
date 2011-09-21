if exist "%PROGRAMFILES(X86)%" goto gotx64 else goto gotx32

:gotx32
set "ARCH_PROGRAMFILES=%PROGRAMFILES%"
goto build

:gotx64
set "ARCH_PROGRAMFILES=%PROGRAMFILES(X86)%"
goto build

:build
CALL "%ARCH_PROGRAMFILES%\Microsoft Visual Studio 9.0\VC\vcvarsall.bat" x86
MSBuild ..\src\uniclient.vcproj /t:Rebuild /p:Configuration=Release
MSBuild PropsCA\PropsCA.vcproj /t:Rebuild /p:Configuration=Release
MSBuild EveAssocCA\EveAssocCA.vcproj /t:Rebuild /p:Configuration=Release

@start /B /WAIT make.py
