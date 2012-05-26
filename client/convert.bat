@echo off

SET ARCH=win32

SETLOCAL

SET QT_PATH=c:\develop\Qt\4.7.4

SET PATH=%PATH%;%QT_PATH%\bin

call "%VS90COMNTOOLS%\..\..\VC\vcvarsall.bat"

@start /B /WAIT convert.py -parents

ENDLOCAL