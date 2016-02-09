set QT=%ENVIRONMENT%/artifacts/qt/5.6.0/windows/x64
set QMAKE=%QT%/bin/qmake.exe
%QMAKE% -config release
jom
