echo Compiling %CONFIGURATION% under %ARCH%
call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" %ARCH%
set QT=%ENVIRONMENT%/packages/windows-%ARCH%/qt-5.6.0
set QMAKE=%QT%/bin/qmake.exe
cd quazip
%QMAKE% -config %CONFIGURATION% quazip_%ARCH%.pro
jom
cd ..
