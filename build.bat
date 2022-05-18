:: Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

@call; &:: Reset errorlevel to ensure that "call ... || exit /b" works as expected.

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" || exit /b

:: Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
@set SOURCE_DIR_WITH_BACKSLASH=%~dp0
@set SOURCE_DIR=%SOURCE_DIR_WITH_BACKSLASH:~0,-1%
@set BUILD_DIR=%SOURCE_DIR%-build

@if exist "%BUILD_DIR%\CMakeCache.txt" goto :skip_generation
    @mkdir "%BUILD_DIR%" 2>NUL

    :: Ensure that only the currently supplied CMake arguments are in effect.
    del "%BUILD_DIR%\CMakeCache.txt" 2>NUL

    cmake "%SOURCE_DIR%" -B "%BUILD_DIR%" -GNinja %* || exit /b
:skip_generation

cmake --build "%BUILD_DIR%"
