@echo off
rem del /F /Q ${libdir}//$arch//bin//${build.configuration}//${project.artifactId}.exe
IF ["${force_x86}"] == ["true"] (set ARCH=x86) ELSE (set ARCH=${arch})

call "%VS110COMNTOOLS%\..\..\VC\vcvarsall.bat" %ARCH%

rem set CONFIG1=%1 
rem set CONFIG2=%2

rem IF NOT [%1] == [] call msbuild common-${arch}.vcproj /t:Rebuild /consoleloggerparameters:ErrorsOnly /p:Configuration=%CONFIG1%
rem IF NOT [%2] == [] call msbuild common-${arch}.vcproj /t:Rebuild /consoleloggerparameters:ErrorsOnly /p:Configuration=%CONFIG2%

call msbuild ${project.build.sourceDirectory}\${project.artifactId}-${arch}.vcxproj /t:Build /consoleloggerparameters:Summary /p:Configuration=${build.configuration}
rem call ${environment.dir}\bin\jom /S