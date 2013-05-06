
set SDK_NAME=nx_sdk
set TARGET_DIR=%SDK_NAME%

rmdir /S /Q %TARGET_DIR%

mkdir %TARGET_DIR%
mkdir %TARGET_DIR%\include\
mkdir %TARGET_DIR%\include\plugins\
mkdir %TARGET_DIR%\sample\
mkdir %TARGET_DIR%\sample\axiscamplugin\
copy /Y readme.txt %TARGET_DIR%\

@rem Copying integration headers
copy /Y ..\common\src\plugins\plugin_api.h %TARGET_DIR%\include\plugins\
copy /Y ..\common\src\plugins\camera_plugin.h %TARGET_DIR%\include\plugins\

@rem Copying AXIS plugin
xcopy /Y ..\axiscamplugin\*.pro %TARGET_DIR%\sample\axiscamplugin\
mkdir %TARGET_DIR%\sample\axiscamplugin\
xcopy /Y ..\axiscamplugin\*.pro %TARGET_DIR%\sample\axiscamplugin\
mkdir %TARGET_DIR%\sample\axiscamplugin\src\
xcopy /Y ..\axiscamplugin\src\*.h %TARGET_DIR%\sample\axiscamplugin\src\
xcopy /Y ..\axiscamplugin\src\*.cpp %TARGET_DIR%\sample\axiscamplugin\src\
copy /Y ..\axiscamplugin\Doxyfile %TARGET_DIR%\sample\axiscamplugin\Doxyfile

@rem Removing unnecessary source files
del /Q /F %TARGET_DIR%\sample\axiscamplugin\src\compatibility_info.cpp

@rem Generating documentation
set CUR_DIR_BAK=%~dp0
cd %TARGET_DIR%\sample\axiscamplugin\
doxygen
cd %CUR_DIR_BAK%
del /F /Q %TARGET_DIR%\sample\axiscamplugin\Doxyfile
