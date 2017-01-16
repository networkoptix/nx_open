@echo off
rem del /F /Q ${libdir}//$arch//bin//${build.configuration}//${project.artifactId}.exe
IF ["${force_x86}"] == ["true"] (set ARCH=x86) ELSE (set ARCH=${arch})

call "%${VCVars}%\..\..\VC\vcvarsall.bat" %ARCH%

rem set CONFIG1=%1 
rem set CONFIG2=%2

rem IF NOT [%1] == [] call msbuild common-${arch}.vcproj /t:Rebuild /consoleloggerparameters:ErrorsOnly /p:Configuration=%CONFIG1%
rem IF NOT [%2] == [] call msbuild common-${arch}.vcproj /t:Rebuild /consoleloggerparameters:ErrorsOnly /p:Configuration=%CONFIG2%

call msbuild ${project.build.sourceDirectory}\${project.artifactId}-${arch}.vcxproj /t:Build /consoleloggerparameters:Summary /p:Configuration=${build.configuration} /p:Platform=${arch}
rem call ${environment.dir}\bin\jom /S
echo build errorlevel is %ERRORLEVEL%
if not [%ERRORLEVEL%] == [0] exit /B %ERRORLEVEL%
if exist ${launcher.version.dir}/${project.artifactId}.exe (
    echo Signing ${project.artifactId}.exe
    call sign.bat ${launcher.version.dir}/${project.artifactId}.exe
) else (
    echo Not signing ${project.artifactId} because it's not executable file
)
exit /B %ERRORLEVEL%