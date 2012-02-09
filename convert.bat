cd common
@start /B /WAIT convert.py
cd %~dp0

cd client
@start /B /WAIT convert.py
cd %~dp0

cd mediaserver
@start /B /WAIT convert.py
cd %~dp0
