call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x64

set CONFIG1=%1 
@start /B /WAIT msbuild launcher-x64.vcxproj /t:Build /consoleloggerparameters:Summary /p:Configuration=%CONFIG1%
if "%ERRORLEVEL%" == "1" exit /B 1

rem Incremental Link-Time Code Generation is a new feature in Visual C++ 2015 
rem (see http://blogs.msdn.com/b/vcblog/archive/2014/11/12/speeding-up-the-incremental-developer-scenario-with-visual-studio-2015.aspx).
echo "Deleting incremental linker files"
DEL /F /Q ..\x64\resources\launcher.iobj ..\x64\resources\launcher.ipdb