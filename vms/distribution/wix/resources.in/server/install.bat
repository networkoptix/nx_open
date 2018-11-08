SET INSTALLER=${server_distribution_name}.exe
cd %~dp0

if exist %INSTALLER% (
    echo Starting "%INSTALLER%"
) else (
    echo File "%INSTALLER%" could not be found!
    exit /b 1
)

:update
    start /wait "%INSTALLER%" /silent /norestart -l "%INSTALLER%.log"
    type "%INSTALLER%.log" "%INSTALLER%_000_ServerPackage.log"
    exit /b

if "%1" != "" (
    call :update >> "%1" 2>&1
) else (
    call :update 2>&1
)
