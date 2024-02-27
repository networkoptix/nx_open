:: Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

@setlocal %= Reset errorlevel and prohibit changing env vars of the parent shell. =%

@if not "%VisualStudioVersion%" == "" goto :vcvars_set
@set VCVARS_BAT="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
@if exist %VCVARS_BAT% goto :vcvars_bat_found
@set VCVARS_BAT="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
@if exist %VCVARS_BAT% goto :vcvars_bat_found
@echo MSVC env vars are not detected, and vcvars64.bat is not found. Fix the installation path of
@echo Visual Studio or Build Tools hard-coded in this script as the value of VCVARS_BAT.
@(call) %= Set ERRORLEVEL to 1 (failure). =%
@goto :exit

:vcvars_bat_found

call %VCVARS_BAT% || @goto :exit

:vcvars_set

:: Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
@set SOURCE_DIR_WITH_BACKSLASH=%~dp0
@set SOURCE_DIR=%SOURCE_DIR_WITH_BACKSLASH:~0,-1%
@set BUILD_DIR=%SOURCE_DIR%-build

@if exist "%BUILD_DIR%\CMakeCache.txt" goto :skip_generation
    @mkdir "%BUILD_DIR%" 2>NUL

    :: Ensure that only the currently supplied CMake arguments are in effect.
    del "%BUILD_DIR%\CMakeCache.txt" 2>NUL

    cmake "%SOURCE_DIR%" -B "%BUILD_DIR%" -GNinja %* || @goto :exit
:skip_generation

cmake --build "%BUILD_DIR%"

:exit
    @exit /b %ERRORLEVEL% %= Needed for a proper cmd.exe exit status. =%
