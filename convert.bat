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

cd appserver
call update.bat
@start /B /WAIT convert.py
cd %~dp0
