:: Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

@echo off
setlocal

:: Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
set BASE_DIR_WITH_BACKSLASH=%~dp0
set BASE_DIR=%BASE_DIR_WITH_BACKSLASH:~0,-1%
set BUILD_DIR=%BASE_DIR%-build

echo on
    if exist "%BUILD_DIR%/" rmdir /S /Q "%BUILD_DIR%/" || goto :end
@echo off

for /d %%S in (%BASE_DIR%\samples\*) do (
    call :build_sample %%S %* || goto :end
)

echo Samples built successfully, see the binaries in %BUILD_DIR%
exit

:end
:: Export the exit status behind endlocal.
endlocal && set RESULT=%ERRORLEVEL%
exit /b %RESULT%

:build_sample
set SOURCE_DIR=%1\src
set SAMPLE=%~n1
set SAMPLE_BUILD_DIR=%BUILD_DIR%\%SAMPLE%
echo on
    mkdir "%SAMPLE_BUILD_DIR%" || goto :build_sample_fail
    cd "%SAMPLE_BUILD_DIR%" || goto :build_sample_fail
    
    cmake "%SOURCE_DIR%" -Ax64 -Tv140,host=x64 %* || goto :build_sample_fail
    cmake --build . || goto :build_sample_fail
@echo off
set ARTIFACT=%SAMPLE_BUILD_DIR%\Debug\%SAMPLE%.dll
if not exist "%ARTIFACT%" (
    echo ERROR: Failed to build plugin %SAMPLE%.
    exit /b 64
)
echo:
echo Plugin built:
echo %ARTIFACT%
echo:
exit /b 0

:build_sample_fail
@echo off && exit /b %ERRORLEVEL%
