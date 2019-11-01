@echo off
set CURRENT_BRANCH=
for /F %%A In ('hg branch') do set CURRENT_BRANCH=%%A
echo Current branch is %CURRENT_BRANCH%
