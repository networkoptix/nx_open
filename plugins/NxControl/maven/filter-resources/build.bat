@echo off
call "%VS110COMNTOOLS%\..\..\VC\vcvarsall.bat" x86
msbuild ${basedir}\NxControl.csproj /t:Build /consoleloggerparameters:Summary /p:Configuration=${build.configuration} /p:Platform=AnyCPU
echo build errorlevel is %ERRORLEVEL%
exit /B %ERRORLEVEL%