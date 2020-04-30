:: Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

@echo off

if [%1] == [/?] goto :show_usage
if [%1] == [-h] goto :show_usage
if [%1] == [--help] goto :show_usage
goto :skip_show_usage
:show_usage
    echo Usage: %~n0%~x0 [--no-qt-samples] [--release] [^<cmake-generation-args^>...]
    echo  --no-qt-samples Exclude samples that require the Qt library.
    echo  --release Compile using Release configuration (with optimizations) instead of Debug.
    echo NOTE: If --no-qt-samples is not specified, and cmake cannot find Qt, add the
    echo  argument to this script (will be passed to the cmake generation command):
    echo  -DCMAKE_PREFIX_PATH=^<absolute-path-to-Qt5-dir^>
    exit /b
:skip_show_usage

:: Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
set BASE_DIR_WITH_BACKSLASH=%~dp0
set BASE_DIR=%BASE_DIR_WITH_BACKSLASH:~0,-1%
set BUILD_DIR=%BASE_DIR%-build

if [%1] == [--no-qt-samples] (
    shift
    set NO_QT_SAMPLES=1
) else (
    set NO_QT_SAMPLES=0
)

if [%1] == [--release] (
    shift
    set BUILD_TYPE=Release
) else (
    set BUILD_TYPE=Debug
)

if [%BUILD_TYPE%] == [Release] (
    set BUILD_OPTIONS=--config %BUILD_TYPE%
)

echo on
    rmdir /S /Q "%BUILD_DIR%/" 2>NUL
@echo off

for /d %%S in (%BASE_DIR%\samples\*) do (
    call :build_sample %%S %1 %2 %3 %4 %5 %6 %7 %8 %9 || @goto :error
)

echo:
echo Samples built successfully, see the binaries in %BUILD_DIR%
exit /b

:build_sample
    set SOURCE_DIR=%1
    set SAMPLE=%~n1
    shift
    if %NO_QT_SAMPLES% == 1 if /i "%SAMPLE%" == "axis_camera_plugin" (
        echo:
        echo ATTENTION: Skipping %SAMPLE% because --no-qt-samples is specified.
        exit /b
    )
    if /i "%SAMPLE:~0,4%" == "rpi_" (
        echo:
        echo ATTENTION: Skipping %SAMPLE% because it is for 32-bit ARM only.
        exit /b
    )
    set SAMPLE_BUILD_DIR=%BUILD_DIR%\%SAMPLE%
    echo on
        mkdir "%SAMPLE_BUILD_DIR%" || @exit /b
        cd "%SAMPLE_BUILD_DIR%" || @exit /b
        
        cmake "%SOURCE_DIR%\src" -Ax64 %1 %2 %3 %4 %5 %6 %7 %8 %9 || @exit /b
        cmake --build . %BUILD_OPTIONS% || @exit /b
    @echo off
    set ARTIFACT=%SAMPLE_BUILD_DIR%\%BUILD_TYPE%\%SAMPLE%.dll
    if not exist "%ARTIFACT%" (
        echo:
        echo ERROR: Failed to build plugin %SAMPLE%.
        exit /b 70
    )
    echo:
    echo Plugin built: %ARTIFACT%
exit /b

:error
    @exit /b %ERRORLEVEL%
