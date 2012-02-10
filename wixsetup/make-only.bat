call "%VS90COMNTOOLS%vsvars32.bat"
call qtvars.bat

set CONFIG=%1

if [%1] == [] set CONFIG=Release

@start /B /WAIT make.py %CONFIG%
