SETLOCAL
call "%VS110COMNTOOLS%\..\..\VC\vcvarsall.bat"

set CONFIG=%1
echo config = %CONFIG%

set ARCH=%2
if [%2] == [] set ARCH=

if [%CONFIG%] == [debug] set OTHER=release
if [%CONFIG%] == [release] set OTHER=debug

set AXHDW=%~dp0
set bebin_path=%AXHDW%\..\build_environment\target\%ARCH%\bin
set PATH=%bebin_path%\%CONFIG%;%PATH%

%bebin_path%\idc %bebin_path%\%CONFIG%\axhdwitness.dll /idl %bebin_path%\%CONFIG%\axhdwitness.idl -version 1.0
midl %bebin_path%\%CONFIG%\axhdwitness.idl /nologo /tlb %bebin_path%\%CONFIG%\axhdwitness.tlb
%bebin_path%\idc %bebin_path%\%CONFIG%\axhdwitness.dll /tlb %bebin_path%\%CONFIG%\axhdwitness.tlb

cd %bebin_path%\%CONFIG%
tlbimp /keyfile:C:\develop\netoptix_vms\ACS\Paxton\SampleKey.snk /out:Interop.hdwitness.dll axhdwitness.dll 
AxImp /keyfile:C:\develop\netoptix_vms\ACS\Paxton\SampleKey.snk /rcw:Interop.hdwitness.dll /out:AxInterop.hdwitness.dll axhdwitness.dll

ENDLOCAL