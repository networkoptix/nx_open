:: Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

@setlocal %= Reset errorlevel and prohibit changing env vars of the parent shell. =%

@set SOURCE_DIR_WITH_BACKSLASH=%~dp0
@set SOURCE_DIR=%SOURCE_DIR_WITH_BACKSLASH:~0,-1%

:: Initialize the MSVC environment.
call %SOURCE_DIR%\build_utils\msvc\call_vcvars64.bat || @goto :exit

:: Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
@set BUILD_DIR=%SOURCE_DIR%-build

@if exist "%BUILD_DIR%\CMakeCache.txt" goto :skip_generation
    @mkdir "%BUILD_DIR%" 2>NUL

    :: Ensure that only the currently supplied CMake arguments are in effect.
    del "%BUILD_DIR%\CMakeCache.txt" 2>NUL

    :: Use the Ninja generator to avoid using CMake compiler search heuristics for MSBuild. Also,
    :: specify the compiler explicitly to avoid potential clashes with gcc if ran from a
    :: Cygwin/MinGW shell.
    GENERATOR_OPTIONS=-GNinja -DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe
    cmake "%SOURCE_DIR%" %GENERATOR_OPTIONS% -B "%BUILD_DIR%" %* || @goto :exit
:skip_generation

cmake --build "%BUILD_DIR%"

:exit
    @exit /b %ERRORLEVEL% %= Needed for a proper cmd.exe exit status. =%
