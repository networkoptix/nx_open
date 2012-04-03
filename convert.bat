cd common
@start /B /WAIT convert.py
cd %~dp0

cd client
@start /B /WAIT convert.py
cd %~dp0

cd mediaserver
@start /B /WAIT convert.py
cd %~dp0

cd traytool
@start /B /WAIT convert.py
cd %~dp0

rem cd appserver
rem call update.bat
rem @start /B /WAIT convert.py
rem cd %~dp0
