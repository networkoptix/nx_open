@echo off

set SOURCE=%NX_RSYNC_SOURCE%
if "%NX_RSYNC_SOURCE%" == "" (
  set SOURCE=enk.me
)

set CURRENTDIR=%cd%
set RSYNC=%CURRENTDIR%\rsync-win32\rsync -rtvL

mkdir %CURRENTDIR%\artifacts\qt\${qt.version}\${platform}\${arch}\${box}
cd %CURRENTDIR%\artifacts\qt\${qt.version}\${platform}\${arch}
%RSYNC% --delete rsync://%SOURCE%/buildenv/artifacts/qt/${qt.version}/${platform}/${arch}/${box} .
cd %CURRENTDIR%
