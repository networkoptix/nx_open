:: Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

@echo off

:: Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
set BASE_DIR_WITH_BACKSLASH=%~dp0
set BASE_DIR=%BASE_DIR_WITH_BACKSLASH:~0,-1%
set BUILD_DIR=%BASE_DIR%-build

if [%1] == [--no-tests] (
    shift
    set NO_TESTS=1
) else (
    set NO_TESTS=0
)
echo on
    rmdir /S /Q "%BUILD_DIR%/" 2>NUL
@echo off

for /d %%S in (%BASE_DIR%\samples\*) do (
    call :build_sample %%S %1 %2 %3 %4 %5 %6 %7 %8 %9 || @goto :error
)

:: Run unit tests if needed.
if [%NO_TESTS%] == [1] echo NOTE: Unit tests were not run. & goto :skip_tests
echo on
    :: Currently, nx_kit and nx_sdk unit tests are built in the scope of stub_analytics_plugin.
    cd "%BUILD_DIR%/stub_analytics_plugin" || @goto :error

    ctest --output-on-failure -C Debug || @goto :error
@echo off
:skip_tests

echo:
echo Samples built successfully, see the binaries in %BUILD_DIR%
exit /b

:build_sample
    set SOURCE_DIR=%1
    set SAMPLE=%~n1
    shift
    if /i "%SAMPLE:~0,4%" == "rpi_" exit /b
    set SAMPLE_BUILD_DIR=%BUILD_DIR%\%SAMPLE%
    echo on
        mkdir "%SAMPLE_BUILD_DIR%" || @exit /b
        cd "%SAMPLE_BUILD_DIR%" || @exit /b
        
        cmake "%SOURCE_DIR%" -Ax64 %1 %2 %3 %4 %5 %6 %7 %8 %9 || @exit /b
        cmake --build . || @exit /b
    @echo off
    set ARTIFACT=%SAMPLE_BUILD_DIR%\Debug\%SAMPLE%.dll
    if not exist "%ARTIFACT%" (
        echo ERROR: Failed to build plugin %SAMPLE%.
        exit /b 70
    )
    echo:
    echo Plugin built: %ARTIFACT%
exit /b

:error
    @exit /b %ERRORLEVEL%
