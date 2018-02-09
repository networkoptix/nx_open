:: Absolute path to this script's dir, including the trailing backslash.
set BASE_DIR=%~dp0

set BUILD_DIR=%BASE_DIR%build
rmdir /S /Q %BUILD_DIR%\
mkdir %BUILD_DIR%
cd %BUILD_DIR%

cmake %BASE_DIR%samples\stub_metadata_plugin -Ax64
cmake --build .

@set ARTIFACT=%BUILD_DIR%\Debug\stub_metadata_plugin.dll
@if exist %ARTIFACT% (
    @echo:
    @echo Plugin built successfully:
    @echo %ARTIFACT%
) else (
    @echo:
    @echo ERROR: Failed to build plugin.
)
