call "%VS90COMNTOOLS%vsvars32.bat"
call qtvars.bat

set CONFIG=%1

if [%1] == [] set CONFIG=Release

call ..\make.bat %CONFIG%

cd ..\appserver\setup
@start /B /WAIT setup.py build
cd %~dp0

MSBuild CustomActions\CustomActions.vcproj /t:Rebuild /p:Configuration=Release

@start /B /WAIT make.py %CONFIG%
