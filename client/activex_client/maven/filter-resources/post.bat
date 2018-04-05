call "%${VCVars}%\..\..\VC\vcvarsall.bat"

set CONFIG=%1
echo config = %CONFIG%

set AXHDW=%~dp0
set bebin_path=${root.dir}\build_environment\target\bin
set PATH=%bebin_path%\%CONFIG%;%PATH%
set IDC=${qt.dir}\bin\idc.exe

set LIBNAME=Ax${paxton.className}${parsedVersion.majorVersion}

echo %IDC% %bebin_path%\%CONFIG%\%LIBNAME%.dll /idl %bebin_path%\%CONFIG%\%LIBNAME%.idl -version ${nxec.ec2ProtoVersion}
%IDC% %bebin_path%\%CONFIG%\%LIBNAME%.dll /idl %bebin_path%\%CONFIG%\%LIBNAME%.idl -version ${nxec.ec2ProtoVersion}

echo midl %bebin_path%\%CONFIG%\%LIBNAME%.idl /nologo /tlb %bebin_path%\%CONFIG%\%LIBNAME%.tlb
midl %bebin_path%\%CONFIG%\%LIBNAME%.idl /nologo /tlb %bebin_path%\%CONFIG%\%LIBNAME%.tlb

echo %IDC% %bebin_path%\%CONFIG%\%LIBNAME%.dll /tlb %bebin_path%\%CONFIG%\%LIBNAME%.tlb
%IDC% %bebin_path%\%CONFIG%\%LIBNAME%.dll /tlb %bebin_path%\%CONFIG%\%LIBNAME%.tlb

cd %bebin_path%\%CONFIG%
tlbimp /keyfile:%AXHDW%\Sign.snk /out:Interop.${paxton.className}_${nxec.ec2ProtoVersion}.dll %LIBNAME%.dll 
regsvr32 /s %LIBNAME%.dll
AxImp /ignoreregisteredocx /keyfile:%AXHDW%\Sign.snk /rcw:Interop.${paxton.className}_${nxec.ec2ProtoVersion}.dll /out:AxInterop.${paxton.className}_${nxec.ec2ProtoVersion}.dll %LIBNAME%.dll
