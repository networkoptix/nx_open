cd ${libdir}//bin//${build.configuration}
FOR /F %%G IN ('dir /b /s') DO ${environment.dir}\bin\mpress.exe -s %%G
${environment.dir}\bin\upx --lzma *.*

cd ${project.build.directory}
FOR /F %%G IN ('dir /b /s *.dll') DO ${environment.dir}\bin\upx --lzma %%G