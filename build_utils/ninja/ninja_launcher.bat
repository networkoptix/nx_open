:: Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

@echo off

if not "%DISABLE_NINJA_TOOL%"=="" GOTO SKIP_NINJA_TOOL_RUN
SET LOG_FILE_NAME=%CD%\build_logs\pre_build.log
python "%~dp0\ninja_tool.py" --log-output "%LOG_FILE_NAME%" --stack-trace
if %ERRORLEVEL% GTR 0 (
    echo Ninja tool exited with error.
    echo For more details, see %LOG_FILE_NAME%
    exit 1
)
:SKIP_NINJA_TOOL_RUN

ninja %*
