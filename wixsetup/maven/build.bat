call "%VS90COMNTOOLS%\..\..\VC\vcvarsall.bat" ${arch}

call msbuild %1 /t:Rebuild /p:Configuration=${build.configuration}