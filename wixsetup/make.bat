CALL "%PROGRAMFILES(X86)%\Microsoft Visual Studio 9.0\VC\vcvarsall.bat" x86
MSBuild ..\src\uniclient.vcproj /t:Rebuild /p:Configuration=Release
MSBuild PropsCA\PropsCA.vcproj /t:Rebuild /p:Configuration=Release

@start /B /WAIT make.py
