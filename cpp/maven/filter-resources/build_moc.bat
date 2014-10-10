cd %1
md build\%2\generated 
${environment.dir}\bin\jom.exe -j 16 /F Makefile.%2 mocables
