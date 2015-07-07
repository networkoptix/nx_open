call "%VS110COMNTOOLS%\..\..\VC\vcvarsall.bat" x86
msbuild ${basedir}\NxInterface.csproj /t:Build /consoleloggerparameters:Summary /p:Configuration=${build.configuration} /p:Platform=AnyCPU
echo build errorlevel is %ERRORLEVEL%
exit /B %ERRORLEVEL%