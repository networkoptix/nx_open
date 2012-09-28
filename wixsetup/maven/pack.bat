cd ${libdir}//bin//${build.configuration}

${environment.dir}\bin\mpress.exe -s client.exe 
FOR /F %%G IN ('dir /b /s *.dll') DO ${environment.dir}\bin\upx -9 --lzma %%G
FOR /F %%G IN ('dir /b /s *.exe') DO ${environment.dir}\bin\upx -9 --lzma %%G

cd ${project.build.directory}
FOR /F %%G IN ('dir /b /s *.dll') DO ${environment.dir}\bin\upx -9 --lzma %%G