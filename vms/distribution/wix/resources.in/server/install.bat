@echo off
SET INSTALLER_FILENAME=${server_distribution_name}
SET INSTALLER=%~dp0%INSTALLER_FILENAME%.exe

if exist %INSTALLER% (
    echo Starting "%INSTALLER%"
) else (
    echo File "%INSTALLER%" could not be found!
    exit /b 1
)

:update
    start "Updating server..." /D %~dp0 /WAIT "%INSTALLER%" /silent /norestart -l "%INSTALLER_FILENAME%.log"
    type "%~dp0%INSTALLER_FILENAME%.log"
    type "%~dp0%INSTALLER_FILENAME%_000_ServerPackage.log"
    exit /b

if "%1" != "" (
    call :update >> "%1" 2>&1
) else (
    call :update 2>&1
)
