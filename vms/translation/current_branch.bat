@echo off
set CURRENT_BRANCH=
for /F %%A In ('git symbolic-ref --short HEAD') do set CURRENT_BRANCH=%%A
echo Current branch is %CURRENT_BRANCH%
