@set ORIGINAL_DIR=%CD%

@set PLUGIN_NAME=stub_metadata_plugin

:: Make the build dir at the same level as the parent dir of this script, suffixed with "-build".
@set BASE_DIR_WITH_BACKSLASH=%~dp0
@set BASE_DIR=%BASE_DIR_WITH_BACKSLASH:~0,-1%
@set BUILD_DIR=%BASE_DIR%-build

@set SOURCE_DIR=%BASE_DIR%\samples\%PLUGIN_NAME%

if exist "%BUILD_DIR%/" rmdir /S /Q "%BUILD_DIR%/" || goto :end
mkdir "%BUILD_DIR%" || goto :end
cd "%BUILD_DIR%" || goto :end

cmake "%SOURCE_DIR%" -Ax64 -Thost=x64 -G "Visual Studio 14 2015" || goto :end
cmake --build . || goto :end

@set ARTIFACT=%BUILD_DIR%\Debug\%PLUGIN_NAME%.dll
@if not exist "%ARTIFACT%" (
    echo ERROR: Failed to build plugin.
    set ERRORLEVEL=42 && goto :end
)

@if [%1] == [--no-tests] echo: && echo NOTE: Unit tests were not run. && goto :skip_tests

ctest --output-on-failure -C Debug || goto :end

:skip_tests

@echo:
@echo Plugin built:
@echo %ARTIFACT%

:end
@set RESULT=%ERRORLEVEL%
@cd %ORIGINAL_DIR%
@exit /b %RESULT%
