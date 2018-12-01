@echo off
SET INSTALLER_FILENAME=${server_distribution_name}
SET INSTALLER=%~dp0%INSTALLER_FILENAME%.exe
SET LOG=%~dp0%INSTALLER_FILENAME%.launch.log

echo %date% %time% Installation was launched with parameter %1 >> %LOG%

if exist %INSTALLER% (
    echo %date% %time% Starting "%INSTALLER%"
    echo %date% %time% Starting "%INSTALLER%" >> %LOG%
) else (
    echo %date% %time% File "%INSTALLER%" could not be found!
    echo %date% %time% File "%INSTALLER%" could not be found! >> %LOG%
    exit /b 1
)

:update
    start "Updating server..." /D %~dp0 /WAIT "%INSTALLER%" /silent /norestart -l "%INSTALLER_FILENAME%.log"
    echo %date% %time% "%INSTALLER%" completed
    echo %date% %time% "%INSTALLER%" completed >> %LOG%
    type "%~dp0%INSTALLER_FILENAME%.log"
    type "%~dp0%INSTALLER_FILENAME%_000_ServerPackage.log"
    exit /b

if "%1" != "" (
    call :update >> "%1" 2>&1
) else (
    call :update 2>&1
)
