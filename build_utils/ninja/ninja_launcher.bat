@echo off

python "%~dp0\..\python\ninja_tool.py" --log-output --stack-trace
ninja %*
