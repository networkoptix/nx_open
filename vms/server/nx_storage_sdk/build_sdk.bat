
set SDK_NAME=nx_storage_sdk
set TARGET_DIR=%SDK_NAME%
set NX_SDK_DIR=..\..\..\libs\nx_sdk

rmdir /S /Q %TARGET_DIR%

mkdir %TARGET_DIR%
mkdir %TARGET_DIR%\include\
mkdir %TARGET_DIR%\include\plugins\
mkdir %TARGET_DIR%\include\plugins\storage\
mkdir %TARGET_DIR%\include\plugins\storage\third_party
mkdir %TARGET_DIR%\sample\
mkdir %TARGET_DIR%\sample\ftpstorageplugin\

copy /Y readme.txt %TARGET_DIR%\

@rem Copying integration headers
copy /Y %NX_SDK_DIR%\src\plugins\plugin_api.h %TARGET_DIR%\include\plugins\
copy /Y ..\nx_vms_server\src\plugins\storage\third_party\third_party_storage.h %TARGET_DIR%\include\plugins\storage\third_party\

@rem Copying Ftp Storage plugin
set PLUGIN_NAME=ftpstorageplugin

mkdir %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
mkdir %TARGET_DIR%\sample\%PLUGIN_NAME%\src\impl

xcopy /Y ..\storage_plugins\%PLUGIN_NAME%\src\CMakeLists.txt %TARGET_DIR%\sample\%PLUGIN_NAME%\src\

xcopy /Y ..\storage_plugins\%PLUGIN_NAME%\src\ftp_library.h %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
xcopy /Y ..\storage_plugins\%PLUGIN_NAME%\src\ftp_library.cpp %TARGET_DIR%\sample\%PLUGIN_NAME%\src\

xcopy /Y ..\storage_plugins\%PLUGIN_NAME%\src\impl\ftplib.h %TARGET_DIR%\sample\%PLUGIN_NAME%\src\impl
xcopy /Y ..\storage_plugins\%PLUGIN_NAME%\src\impl\ftplib.cpp %TARGET_DIR%\sample\%PLUGIN_NAME%\src\impl

copy /Y ..\storage_plugins\%PLUGIN_NAME%\Doxyfile %TARGET_DIR%\sample\%PLUGIN_NAME%\Doxyfile

@rem Generating documentation
set CUR_DIR_BAK=%~dp0
cd %TARGET_DIR%\sample\%PLUGIN_NAME%\
%environment%\bin\doxygen
cd %CUR_DIR_BAK%
del /F /Q %TARGET_DIR%\sample\%PLUGIN_NAME%\Doxyfile
