call "%VS90COMNTOOLS%\..\..\VC\vcvarsall.bat" x64

set CONFIG1=%1 
@start /B /WAIT  msbuild launcher-x64.vcproj /t:Build /consoleloggerparameters:Summary /p:Configuration=%CONFIG1%
if "%ERRORLEVEL%" == "1" exit /B 1