@echo off
rem For reasons unknown, qmake generates invalid project files for VS2010.
rem So we generate project files for VS2008, and then convert them for use with VS2010.
set QMAKESPEC=win32-msvc2008
start /B /WAIT convert.py -parents
set QMAKESPEC=

erase /Q src\eveclient.vcxproj
erase /Q src\eveclient.vcxproj.filters
devenv /upgrade src\eveclient.vcproj
rmdir /S /Q src\Backup
rmdir /S /Q src\_UpgradeReport_Files
erase /Q src\UpgradeLog*.xml

erase /Q ..\eveCommon\src\common.vcxproj
erase /Q ..\eveCommon\src\common.vcxproj.filters
devenv /upgrade ..\eveCommon\src\common.vcproj
rmdir /S /Q ..\eveCommon\src\Backup
rmdir /S /Q ..\eveCommon\src\_UpgradeReport_Files
erase /Q ..\eveCommon\src\UpgradeLog*.xml

