set PLUGIN_NAME=stub_metadata_plugin

:: Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
set BASE_DIR_WITH_BACKSLASH=%~dp0
set BASE_DIR=%BASE_DIR_WITH_BACKSLASH:~0,-1%
set BUILD_DIR=%BASE_DIR%-build

if exist "%BUILD_DIR%/" rmdir /S /Q "%BUILD_DIR%/" || goto :EOF
mkdir "%BUILD_DIR%" || goto :EOF
@set ORIGINAL_DIR=%cd%
cd "%BUILD_DIR%" || goto :EOF

cmake "%BASE_DIR%\samples\%PLUGIN_NAME%" -Ax64 || goto :restoreCd
cmake --build . || goto :restoreCd

@set ARTIFACT=%BUILD_DIR%\Debug\%PLUGIN_NAME%.dll
@if exist "%ARTIFACT%" (
    @echo:
    @echo Plugin built successfully:
    @echo %ARTIFACT%
) else (
    @echo:
    @echo ERROR: Failed to build plugin.
)

:restoreCd
@cd %ORIGINAL_DIR%
