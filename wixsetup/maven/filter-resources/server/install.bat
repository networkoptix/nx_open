REM place %FILE%.msi here

SET FILE=${finalName}-server-only.msi

:update
    msiexec.exe /quiet /norestart /l*v %FILE%.log /i %FILE% INSTALL_SERVER=1 INSTALL_CLIENT=0
    type %FILE%.log
    exit /b

if "%1" != "" (
    call :update >> "%1" 2>&1
) else (
    call :update 2>&1
)