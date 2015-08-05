
set SDK_NAME=nx_storage_sdk
set TARGET_DIR=%SDK_NAME%

rmdir /S /Q %TARGET_DIR%

mkdir %TARGET_DIR%
mkdir %TARGET_DIR%\include\
mkdir %TARGET_DIR%\include\plugins\
mkdir %TARGET_DIR%\sample\
mkdir %TARGET_DIR%\sample\ftpstorageplugin\
copy /Y readme.txt %TARGET_DIR%\

@rem Copying integration headers
copy /Y ..\common\src\plugins\plugin_api.h %TARGET_DIR%\include\plugins\
copy /Y ..\common\src\plugins\storage\third_party\third_party_storage.h %TARGET_DIR%\include\

@rem Copying Ftp Storage plugin
set PLUGIN_NAME=ftpstorageplugin

xcopy /Y ..\storage_plugins\%PLUGIN_NAME%\*.pro %TARGET_DIR%\sample\%PLUGIN_NAME%\
mkdir %TARGET_DIR%\sample\%PLUGIN_NAME%\
xcopy /Y ..\storage_plugins\%PLUGIN_NAME%\*.pro %TARGET_DIR%\sample\%PLUGIN_NAME%\
mkdir %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
mkdir %TARGET_DIR%\sample\%PLUGIN_NAME%\src\impl
xcopy /Y ..\plugins\%PLUGIN_NAME%\src\*.h %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
xcopy /Y ..\plugins\%PLUGIN_NAME%\src\*.cpp %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
xcopy /Y ..\plugins\%PLUGIN_NAME%\src\impl*.h %TARGET_DIR%\sample\%PLUGIN_NAME%\src\impl
xcopy /Y ..\plugins\%PLUGIN_NAME%\src\impl*.cpp %TARGET_DIR%\sample\%PLUGIN_NAME%\src\impl
copy /Y ..\plugins\%PLUGIN_NAME%\Doxyfile %TARGET_DIR%\sample\%PLUGIN_NAME%\Doxyfile

@rem Removing unnecessary source files
del /Q /F %TARGET_DIR%\sample\%PLUGIN_NAME%\src\compatibility_info.cpp

@rem Generating documentation
set CUR_DIR_BAK=%~dp0
cd %TARGET_DIR%\sample\%PLUGIN_NAME%\
%environment%\bin\doxygen
cd %CUR_DIR_BAK%
del /F /Q %TARGET_DIR%\sample\%PLUGIN_NAME%\Doxyfile
