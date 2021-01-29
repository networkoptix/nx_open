@echo off

if not "%DISABLE_NINJA_TOOL%"=="" GOTO SKIP_NINJA_TOOL_RUN
python "%~dp0\..\python\ninja_tool.py" --log-output --stack-trace
:SKIP_NINJA_TOOL_RUN

ninja %*
