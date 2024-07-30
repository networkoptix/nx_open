:: Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

:: Since there are bugs in the latest MSVC compiler (and, also, ABI incompatibilities with the
:: previous versions), we need more fine control over the toolset version used to compile the SDK
:: samples. This script provides such control - it is a wrapper around vcvars64.bat that allows
:: specifying the MSVC version to use. If the version is not specified, the script will try to find
:: the latest compatible version installed on the system.

@echo off

set INCOMPATIBLE_VC_TOOLSET_VERSION=14.39

set VCVARS_BAT=
set VC_TOOLSET_VERSION=

if not "%NX_FORCED_VC_TOOLSET_VERSION%" == "" (
    echo Forcing MSVC version %NX_FORCED_VC_TOOLSET_VERSION%.
    echo: %= Print an empty line. =%
)

:: Check if MSVC environment is already initialized for the suitable version.
if "%VCToolsVersion%" == "" goto :env_not_intialized

:: This check works only for exact match. If NX_FORCED_VC_TOOLSET_VERSION, for example, is 14.29
:: and VCToolsVersion is 14.29.30133, the check will fail. This is not really a problem - we will
:: just reinitialize the environment with the forced version. TODO: Try to figure out a better way
:: to compare the versions.
if not "%NX_FORCED_VC_TOOLSET_VERSION%" == "" (
    if not "%VCToolsVersion%" == "%NX_FORCED_VC_TOOLSET_VERSION%" (
        goto :env_intialized_with_incompatible_version
    )
)

if "%VCToolsVersion%" lss "%INCOMPATIBLE_VC_TOOLSET_VERSION%" (
    echo MSVC environment is already initialized for version %VCToolsVersion%.
    echo: %= Print an empty line. =%
    goto :exit
)

:env_intialized_with_incompatible_version

echo MSVC environment is initialized for version %VCToolsVersion%. This version is incompatible
echo with the project - trying to find a suitable version
echo: %= Print an empty line. =%

:env_not_intialized

:: Reset errorlevel and prohibit changing env vars of the parent shell.
setlocal EnableDelayedExpansion %= Enable delayed expansion for variables. =%

:: Find the vcvars64.bat script and the toolset directory. Check for then in the typical Build
:: Tools installation location first, then try Visual Studio Community edition installation location.
set _VCVARS_BAT="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
set TOOLS_DIRECTORY="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
if exist %_VCVARS_BAT% if exist %TOOLS_DIRECTORY% goto :msvc_found

set _VCVARS_BAT="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set TOOLS_DIRECTORY="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
if exist %_VCVARS_BAT% if exist %TOOLS_DIRECTORY% goto :msvc_found

echo vcvars64.bat or VC++ toolset directory is not found. Fix the installation path of Visual
echo Studio or Build Tools hard-coded in this script as the values of _VCVARS_BAT and
echo TOOLS_DIRECTORY.
(call) %= Set ERRORLEVEL to 1 (failure). =%
goto :exit

:msvc_found

set _VC_TOOLSET_VERSION=%NX_FORCED_VC_TOOLSET_VERSION%
if "%_VC_TOOLSET_VERSION%" == "" (
    :: List all the installed toolsets.
    for /f "delims=" %%F in ('dir /b %TOOLS_DIRECTORY%\*') do (
        :: Check if the first incompatible version is greater than the next toolset version.
        if "!INCOMPATIBLE_VC_TOOLSET_VERSION!" gtr "%%F" (
            :: Check if the maximum found toolset version is less than the next toolset version.
            if "!_VC_TOOLSET_VERSION!" lss "%%F" (
                set "_VC_TOOLSET_VERSION=%%F"
            )
        )
    )
) else (
    set _VC_TOOLSET_VERSION=%NX_FORCED_VC_TOOLSET_VERSION%
)

if "!_VC_TOOLSET_VERSION!" == "" (
    @echo Suitable MSVC version is not found. Please, set up the environment manually.
    @(call) %= Set ERRORLEVEL to 1 (failure). =%
    @goto :exit
)

endlocal & set "VCVARS_BAT=%_VCVARS_BAT%" & set "VC_TOOLSET_VERSION=%_VC_TOOLSET_VERSION%"

:call_vcvars

echo Initializaing MSVC environment for version %VC_TOOLSET_VERSION%

@echo on
call %VCVARS_BAT% --vcvars_ver=%VC_TOOLSET_VERSION% || @goto :exit
@echo off

echo: %= Print an empty line. =%
echo MSVC environment is initialized for version %VC_TOOLSET_VERSION%.

:exit
    @exit /b %ERRORLEVEL% %= Needed for a proper cmd.exe exit status. =%
