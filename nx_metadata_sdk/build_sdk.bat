set SDK_NAME=nx_metadata_sdk
set TARGET_DIR=%SDK_NAME%

rmdir /S /Q %TARGET_DIR%
mkdir %TARGET_DIR%

copy readme.md %TARGET_DIR%\
copy build_sample.bat %TARGET_DIR%\
copy build_sample.sh %TARGET_DIR%\

@rem Copying nx_kit and removing unneeded part of the copyright notice
set NX_KIT_DST=%TARGET_DIR%\nx_kit
xcopy /S ..\open\artifacts\nx_kit %NX_KIT_DST%\
@rem ATTENTION: For the following statements, Cygwin is required, and "repl" tool.
set REPL=~/develop/devtools/util/repl.sh
set TEXT_TO_REPLACE=" Licensed under GNU Lesser General Public License version 3."
zsh -c '%REPL% %TEXT_TO_REPLACE% "" "%NX_KIT_DST%"/**/*'
zsh -c 'rm "%NX_KIT_DST%"/**/*.repl.BAK'

@rem Copying integration headers and sources
set PLUGINS_SRC=..\common\src\plugins
set PLUGINS_DST=%TARGET_DIR%\src\plugins
xcopy /S %PLUGINS_SRC%\plugin_api.h %PLUGINS_DST%\
xcopy /S %PLUGINS_SRC%\plugin_tools.h %PLUGINS_DST%\

set METADATA_DST=%TARGET_DIR%\src\nx\sdk\metadata
xcopy /S ..\common\src\nx\sdk\metadata %METADATA_DST%\
copy ..\common\src\nx\sdk\common.h %TARGET_DIR%\src\nx\sdk\
del %METADATA_DST%\media_frame.h
del %METADATA_DST%\audio_frame.h
del %METADATA_DST%\video_frame.h

@rem Copying Stub Metadata Plugin
set PLUGIN_NAME=stub_metadata_plugin
set PLUGIN_DST=%TARGET_DIR%\samples\%PLUGIN_NAME%
xcopy /S ..\plugins\metadata\%PLUGIN_NAME%\src %PLUGIN_DST%\src\
copy CMakeLists.txt %PLUGIN_DST%\

set DOXYFILE=%TARGET_DIR%\Doxyfile
copy Doxyfile %DOXYFILE%

@rem Generating documentation
set CUR_DIR_BAK=%~dp0
cd %TARGET_DIR%
%environment%\bin\doxygen
cd %CUR_DIR_BAK%
del %DOXYFILE%

@rem Creating .zip
set ZIP_ARCHIVE=%SDK_NAME%.zip
del %ZIP_ARCHIVE%
@rem ATTENTION: For the following statement, Cygwin is required.
zsh -c 'zip -r %ZIP_ARCHIVE% %TARGET_DIR%'
