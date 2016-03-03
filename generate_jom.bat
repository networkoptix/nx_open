mkdir build-jom
cd build-jom
call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" X64
cmake -G "NMake Makefiles JOM" ..