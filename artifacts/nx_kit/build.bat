:: Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

@echo off
setlocal %= Reset errorlevel and prohibit changing env vars of the parent shell. =%

if [%1] == [/?] goto :show_usage
if [%1] == [-h] goto :show_usage
if [%1] == [--help] goto :show_usage
goto :skip_show_usage
:show_usage
    echo Usage: %~n0%~x0 [--no-tests] [--debug] [^<cmake-generation-args^>...]
    echo  --debug Compile using Debug configuration (without optimizations) instead of Release.
    goto :exit
:skip_show_usage

:: Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
set BASE_DIR_WITH_BACKSLASH=%~dp0
set BASE_DIR=%BASE_DIR_WITH_BACKSLASH:~0,-1%
set BUILD_DIR=%BASE_DIR%-build

:: Try to find the vcvars64.bat script, assuming that build_nx_kit.bat runs from the
:: "open/artifacts/nx_kit" subdirectory of the nx repository.
set VCVARS_BAT=%BASE_DIR%\..\..\build_utils\msvc\call_vcvars64.bat
if exist "%VCVARS_BAT%" (
    echo on
    call "%VCVARS_BAT%" || goto :exit
    @echo off
)

if [%1] == [--no-tests] (
    shift
    set NO_TESTS=1
) else (
    set NO_TESTS=0
)

if [%1] == [--debug] (
    shift
    set BUILD_TYPE=Debug
) else (
    set BUILD_TYPE=Release
)

set GENERATOR_OPTIONS=-GNinja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe

echo on
rmdir /S /Q "%BUILD_DIR%" 2>NUL
mkdir "%BUILD_DIR%" || goto :exit
cmake "%BASE_DIR%" -B "%BUILD_DIR%" %GENERATOR_OPTIONS% %1 %2 %3 %4 %5 %6 %7 %8 %9 || goto :exit
cmake --build "%BUILD_DIR%" || goto :exit
@echo off

:: Run unit tests if needed.
if [%NO_TESTS%] == [1] echo NOTE: Unit tests were not run. & goto :skip_tests

cd "%BUILD_DIR%\unit_tests"
setlocal
PATH=%BUILD_DIR%;%PATH%
echo on
ctest --output-on-failure -C "${BUILD_TYPE}" || @goto :exit
@echo off
endlocal

:skip_tests

echo:
echo Nx kit built successfully, see the library in %BUILD_DIR%

:exit
    exit /b %ERRORLEVEL% %= Needed for a proper cmd.exe exit status. =%
