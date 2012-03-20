call "%VS90COMNTOOLS%vsvars32.bat"
call qtvars.bat

set CONFIG=%1

if [%1] == [] set CONFIG=Release

if not exist ..\appserver\setup\build\exe.win32-2.7\ec.exe (
cd ..\appserver\setup
@start /B /WAIT setup.py build
cd %~dp0
)

MSBuild CustomActions\CustomActions.vcproj /t:Build /p:Configuration=Release

@start /B /WAIT make.py %CONFIG%
