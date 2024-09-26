:: Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

@echo off
setlocal %= Reset errorlevel and prohibit changing env vars of the parent shell. =%

if [%1] == [/?] goto :show_usage
if [%1] == [-h] goto :show_usage
if [%1] == [--help] goto :show_usage
goto :skip_show_usage
:show_usage
    echo Usage: %~n0%~x0 [^<cmake-generation-args^>...]
    echo To avoid running unit tests, set the environment variable NX_SDK_NO_TESTS=1.
    echo:
    echo To build samples using the Debug configuration, use -DCMAKE_BUILD_TYPE=Debug.
    echo:
    echo To build samples that require Qt, pass the paths to the Qt and Qt Host Conan packages using
    echo  CMake parameters:
    echo  -DQT_DIR=^<path-to-qt-conan-package^> -DQT_HOST_PATH=^<path-to-qt-host-conan-package^>
    goto :exit
:skip_show_usage

:: Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
set BASE_DIR_WITH_BACKSLASH=%~dp0
set BASE_DIR=%BASE_DIR_WITH_BACKSLASH:~0,-1%
set BUILD_DIR=%BASE_DIR%-build

echo on
call "%BASE_DIR%/call_vcvars64.bat" || goto :exit
@echo off

:: Use Ninja generator to avoid using CMake compiler search heuristics for MSBuild. Also, specify
:: the compiler explicitly to avoid potential clashes with gcc if run from a Cygwin/MinGW/GitBash
:: shell.
set GENERATOR_OPTIONS=-GNinja -DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe

echo on
rmdir /S /Q "%BUILD_DIR%" 2>NUL
@echo off

cmake "%BASE_DIR%\samples" -B "%BUILD_DIR%" %GENERATOR_OPTIONS% %* || @exit /b
cmake --build "%BUILD_DIR%" || @exit /b

:: Run unit tests if needed.
if [%NX_SDK_NO_TESTS%] == [1] echo NOTE: Unit tests were not run. & goto :skip_tests
echo on
cd %BUILD_DIR%
ctest --output-on-failure || @goto :exit
@echo off

:skip_tests

echo:
echo Samples built successfully, see the binaries in %BUILD_DIR%\samples\

:exit
    exit /b %ERRORLEVEL% %= Needed for a proper cmd.exe exit status. =%
