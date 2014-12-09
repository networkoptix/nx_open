SETLOCAL
call "%VS100COMNTOOLS%\..\..\VC\vcvarsall.bat"

set CONFIG=%1
echo config = %CONFIG%

set TARGETPATH=%2
if [%2] == [] set TARGETPATH=

if [%CONFIG%] == [debug] set OTHER=release
if [%CONFIG%] == [release] set OTHER=debug

set AXHDW=%~dp0
set PATH=%AXHDW%\%TARGETPATH%\bin\%CONFIG%;%PATH%

idc %AXHDW%\%TARGETPATH%\bin\%CONFIG%\axhdwitness.dll /idl %TARGETPATH%\build\%CONFIG%\axhdwitness.idl -version 1.0
midl %TARGETPATH%\build\%CONFIG%\axhdwitness.idl /nologo /tlb %TARGETPATH%\build\%CONFIG%\axhdwitness.tlb
idc %AXHDW%\%TARGETPATH%\bin\%CONFIG%\axhdwitness.dll /tlb %TARGETPATH%\build\%CONFIG%\axhdwitness.tlb
ENDLOCAL