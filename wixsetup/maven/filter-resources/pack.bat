@echo off

set CURRENTDIR=%CD%
set PACKDIR=%CURRENTDIR%\pack
cd %PACKDIR%
FOR /F %%G IN ('dir /b /AD') DO (
  echo %%G
  cd %%G
  start /B /HIGH cmd.exe /C ${environment.dir}\bin\7z.exe a -sdel ..\%%G.zip *
  cd %PACKDIR%
  SET TESTRESULT=%ERRORLEVEL%
  echo pack errorlevel is %ERRORLEVEL%
)
cd %CURRENTDIR%
