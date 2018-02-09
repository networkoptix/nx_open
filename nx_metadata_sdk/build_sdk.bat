:: Absolute path to this script's dir, including the trailing backslash.
set BASE_DIR=%~dp0

set VMS_DIR=%BASE_DIR%..
set SDK_NAME=nx_metadata_sdk
set TARGET_DIR=%BASE_DIR%%SDK_NAME%

rmdir /S /Q %TARGET_DIR%
mkdir %TARGET_DIR%

xcopy /S %BASE_DIR%metadata_sdk\* %TARGET_DIR%\

:: Copying nx_kit and removing unneeded part of the copyright notice
set NX_KIT_DST=%TARGET_DIR%\nx_kit
xcopy /S %VMS_DIR%\open\artifacts\nx_kit %NX_KIT_DST%\
:: ATTENTION: For the following statements, Cygwin is required, and "repl" tool.
set REPL=~/develop/devtools/util/repl.sh
set TEXT_TO_REPLACE=" Licensed under GNU Lesser General Public License version 3."
zsh -c '%REPL% %TEXT_TO_REPLACE% "" "%NX_KIT_DST%"/**/*'
zsh -c 'rm "%NX_KIT_DST%"/**/*.repl.BAK'

:: Copying integration headers and sources
set PLUGINS_SRC=%VMS_DIR%\common\src\plugins
set PLUGINS_DST=%TARGET_DIR%\src\plugins
xcopy /S %PLUGINS_SRC%\plugin_api.h %PLUGINS_DST%\
xcopy /S %PLUGINS_SRC%\plugin_tools.h %PLUGINS_DST%\

set METADATA_DST=%TARGET_DIR%\src\nx\sdk\metadata
xcopy /S %VMS_DIR%\common\src\nx\sdk\metadata %METADATA_DST%\
copy %VMS_DIR%\common\src\nx\sdk\common.h %TARGET_DIR%\src\nx\sdk\
del %METADATA_DST%\media_frame.h
del %METADATA_DST%\audio_frame.h
del %METADATA_DST%\video_frame.h

:: Copying Stub Metadata Plugin
set PLUGIN_NAME=stub_metadata_plugin
set PLUGIN_DST=%TARGET_DIR%\samples\%PLUGIN_NAME%
xcopy /S %VMS_DIR%\plugins\metadata\%PLUGIN_NAME%\src %PLUGIN_DST%\src\

set DOXYFILE=%TARGET_DIR%\Doxyfile

:: Generating documentation
cd %TARGET_DIR%
%environment%\bin\doxygen
cd %BASE_DIR%
del %DOXYFILE%

:: Creating .zip
set ZIP_ARCHIVE=%BASE_DIR%%SDK_NAME%.zip
del %ZIP_ARCHIVE%
cd %TARGET_DIR%\..
:: ATTENTION: For the following statement, Cygwin is required.
zsh -c 'zip -r "$(cygpath -u "%ZIP_ARCHIVE%")" "%SDK_NAME%"'
