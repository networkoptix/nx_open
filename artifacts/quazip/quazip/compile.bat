set ZLIB=../../zlib
set QT=%ENVIRONMENT%/artifacts/qt/5.6.0/windows/x64
set QMAKE=%QT%/bin/qmake.exe
%QMAKE% -project
%QMAKE% LIBS+=%ZLIB%/lib/zdll.lib LIBS+=%QT%/lib/Qt5Core.lib LIBS+=%QT%/lib/Qt5Cored.lib INCLUDEPATH+=%ZLIB%/include INCLUDEPATH+=%QT%/include INCLUDEPATH+=%QT%/include/QtCore -config release
jom
