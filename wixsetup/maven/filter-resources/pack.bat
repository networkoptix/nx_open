@echo off
rem cd ${libdir}//bin//${build.configuration}

rem ${environment.dir}\bin\mpress.exe -s client.exe 
rem FOR /F %%G IN ('dir /b /s *.dll') DO ${environment.dir}\bin\upx -9 --lzma %%G
rem FOR /F %%G IN ('dir /b /s *.exe') DO ${environment.dir}\bin\upx -9 --lzma %%G

rem cd ${project.build.directory}
rem FOR /F %%G IN ('dir /b /s *.dll') DO ${environment.dir}\bin\upx -9 --lzma %%G

set CURRENTDIR=%CD%
set PACKDIR=%CURRENTDIR%\pack
cd %PACKDIR%
FOR /F %%G IN ('dir /b /AD') DO (
  echo %%G
  cd %%G 
  start /B /HIGH cmd.exe /C ${environment.dir}\bin\7z.exe a ..\%%G.zip *
  cd %PACKDIR% 
  SET TESTRESULT=%ERRORLEVEL%
  echo pack errorlevel is %ERRORLEVEL%
  rem del /F /S /Q %%G
  )
cd %CURRENTDIR%  