SET FILE=${artifact.name.server}.exe

:update
    start /wait %FILE% /silent /norestart -l %FILE%.log
    type %FILE%.log %FILE%_000_ServerPackage.log
    exit /b

if "%1" != "" (
    call :update >> "%1" 2>&1
) else (
    call :update 2>&1
)