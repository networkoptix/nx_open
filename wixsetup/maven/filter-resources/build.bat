call "%${VCVars}%\..\..\VC\vcvarsall.bat" ${arch}

call msbuild CustomActions-${arch}.vcxproj /t:Rebuild /p:Configuration=Release