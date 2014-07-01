call "%VS110COMNTOOLS%\..\..\VC\vcvarsall.bat" ${arch}

call msbuild CustomActions-${arch}.vcxproj /t:Rebuild /p:Configuration=Debug