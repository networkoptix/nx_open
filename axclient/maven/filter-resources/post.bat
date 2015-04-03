call "%VS110COMNTOOLS%\..\..\VC\vcvarsall.bat"

set CONFIG=%1
echo config = %CONFIG%

set ARCH=%2
if [%2] == [] set ARCH=

if [%CONFIG%] == [debug] set OTHER=release
if [%CONFIG%] == [release] set OTHER=debug

set AXHDW=%~dp0
set bebin_path=%AXHDW%\..\..\build_environment\target\%ARCH%\bin
set PATH=%bebin_path%\%CONFIG%;%PATH%

set LIBNAME=Ax${ax.className}2

%bebin_path%\idc %bebin_path%\%CONFIG%\%LIBNAME%.dll /idl %bebin_path%\%CONFIG%\%LIBNAME%.idl -version ${nxec.ec2ProtoVersion}
midl %bebin_path%\%CONFIG%\%LIBNAME%.idl /nologo /tlb %bebin_path%\%CONFIG%\%LIBNAME%.tlb
%bebin_path%\idc %bebin_path%\%CONFIG%\%LIBNAME%.dll /tlb %bebin_path%\%CONFIG%\%LIBNAME%.tlb

cd %bebin_path%\%CONFIG%
tlbimp /keyfile:%AXHDW%\Sign.snk /out:Interop.${ax.className}.dll %LIBNAME%.dll 
regsvr32 /s %LIBNAME%.dll
AxImp /keyfile:%AXHDW%\Sign.snk /rcw:Interop.${ax.className}.dll /out:AxInterop.${ax.className}.dll %LIBNAME%.dll