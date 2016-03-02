mkdir build-vs
cd build-vs
call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" x64
cmake "Visual Studio 14 2015 Win64" ..