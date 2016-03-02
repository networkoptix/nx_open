@echo off

set SOURCE=%NX_RSYNC_SOURCE%
if "%NX_RSYNC_SOURCE%" == "" (
  set SOURCE=enk.me
)
set QT_PATH=%SOURCE%/buildenv/${qt.path}

set BUILDENVDIR=%cd%
set RSYNC=%BUILDENVDIR%\rsync-win32\rsync -rtvL --chmod=ugo=rwx

set qtdir=%BUILDENVDIR%\${qt.path}
rem replacing / with \
set qtdir=%qtdir:/=\%

mkdir %qtdir%
cd %qtdir%

@setlocal enableextensions enabledelayedexpansion
@echo off
set debug_mode="${qt.debug}"
cd ..
if not x%debug_mode:pdb=%==x%debug_mode% (
    %RSYNC% --delete rsync://%QT_PATH% .
) else (
    %RSYNC% --delete --exclude '*.pdb' rsync://%QT_PATH% .
)
cd %qtdir%

if not x%debug_mode:src=%==x%debug_mode% (
    cd %BUILDENVDIR%\artifacts\qt\${qt.version}
    %RSYNC% --delete rsync://%SOURCE%/buildenv/artifacts/qt/${qt.version}/src .
)
    
endlocal

cd %BUILDENVDIR%
