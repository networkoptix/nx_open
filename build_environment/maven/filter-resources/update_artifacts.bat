@echo off

set SOURCE=%NX_RSYNC_SOURCE%
if "%NX_RSYNC_SOURCE%" == "" (
  set SOURCE=enk.me
)
set QT_PATH=%SOURCE%/buildenv/artifacts/qt/${qt.version}/${platform}/${arch}

set BUILDENVDIR=%cd%
set RSYNC=%BUILDENVDIR%\rsync-win32\rsync -rtvL --chmod=ugo=rwx

mkdir %BUILDENVDIR%\artifacts\qt\${qt.version}\${platform}\${arch}
cd %BUILDENVDIR%\artifacts\qt\${qt.version}\${platform}

@setlocal enableextensions enabledelayedexpansion
@echo off
set debug_mode="${qt.debug}"
if not x%debug_mode:pdb=%==x%debug_mode% (
    %RSYNC% --delete rsync://%QT_PATH% .
) else (
    %RSYNC% --delete --exclude '*.pdb' rsync://%QT_PATH% .
)

if not x%debug_mode:src=%==x%debug_mode% (
    cd %BUILDENVDIR%\artifacts\qt\${qt.version}
    %RSYNC% --delete rsync://%SOURCE%/buildenv/artifacts/qt/${qt.version}/src .
)
    
endlocal

cd %BUILDENVDIR%

