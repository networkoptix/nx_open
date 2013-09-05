@echo off
rem del /F /Q ${libdir}//$arch//bin//${build.configuration}//${artifactId}.exe
call "%VS110COMNTOOLS%\..\..\VC\vcvarsall.bat" ${arch}

rem set CONFIG1=%1 
rem set CONFIG2=%2

rem IF NOT [%1] == [] call msbuild common-${arch}.vcproj /t:Rebuild /consoleloggerparameters:ErrorsOnly /p:Configuration=%CONFIG1%
rem IF NOT [%2] == [] call msbuild common-${arch}.vcproj /t:Rebuild /consoleloggerparameters:ErrorsOnly /p:Configuration=%CONFIG2%

call msbuild ${project.build.sourceDirectory}\${artifactId}-${arch}.vcxproj /t:Build /consoleloggerparameters:Summary /p:Configuration=${build.configuration}