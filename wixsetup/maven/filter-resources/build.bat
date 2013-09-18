call "%VS90COMNTOOLS%\..\..\VC\vcvarsall.bat" ${arch}

call msbuild CustomActions-${arch}.vcproj /t:Rebuild /p:Configuration=${build.configuration}