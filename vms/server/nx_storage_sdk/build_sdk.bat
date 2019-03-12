set PLUGIN_NAME=ftpstorageplugin
set TARGET_DIR=..\nx_storage_sdk-build
set NX_SDK_DIR=vms\libs\nx_sdk
set PLUGIN_DIR=vms\server\storage_plugins\%PLUGIN_NAME%

rmdir /S /Q %TARGET_DIR%

mkdir %TARGET_DIR%
mkdir %TARGET_DIR%\include\
mkdir %TARGET_DIR%\include\plugins\
mkdir %TARGET_DIR%\include\storage\
mkdir %TARGET_DIR%\sample\
mkdir %TARGET_DIR%\sample\ftpstorageplugin\

copy /Y readme.txt %TARGET_DIR%\

@rem Copying integration headers
copy /Y %NX_SDK_DIR%\src\plugins\plugin_api.h %TARGET_DIR%\include\plugins\
copy /Y vms\libs\nx_sdk\src\storage\third_party_storage.h %TARGET_DIR%\include\storage\

@rem Copying Ftp Storage plugin

mkdir %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
mkdir %TARGET_DIR%\sample\%PLUGIN_NAME%\src\impl

xcopy /Y %PLUGIN_DIR%\src\CMakeLists.txt %TARGET_DIR%\sample\%PLUGIN_NAME%\src\

xcopy /Y %PLUGIN_DIR%\src\ftp_library.h %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
xcopy /Y %PLUGIN_DIR%\src\ftp_library.cpp %TARGET_DIR%\sample\%PLUGIN_NAME%\src\

xcopy /Y %PLUGIN_DIR%\src\impl\ftplib.h %TARGET_DIR%\sample\%PLUGIN_NAME%\src\impl
xcopy /Y %PLUGIN_DIR%\src\impl\ftplib.cpp %TARGET_DIR%\sample\%PLUGIN_NAME%\src\impl

copy /Y %PLUGIN_DIR%\Doxyfile %TARGET_DIR%\sample\%PLUGIN_NAME%\Doxyfile

@rem Generating documentation
set CUR_DIR_BAK=%cd%
cd %TARGET_DIR%\sample\%PLUGIN_NAME%\
doxygen
cd %CUR_DIR_BAK%
del /F /Q %TARGET_DIR%\sample\%PLUGIN_NAME%\Doxyfile

echo Built: %TARGET_DIR%
