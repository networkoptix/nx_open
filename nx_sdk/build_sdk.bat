
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
copy /Y ..\common_libs\nx_sdk\src\plugins\plugin_api.h %TARGET_DIR%\include\plugins\
copy /Y ..\common_libs\nx_sdk\src\plugins\plugin_tools.h %TARGET_DIR%\include\plugins\
copy /Y ..\common_libs\nx_sdk\src\plugins\plugin_container_api.h %TARGET_DIR%\include\plugins\
copy /Y ..\common_libs\nx_sdk\src\camera\camera_plugin.h %TARGET_DIR%\include\plugins\
copy /Y ..\common_libs\nx_sdk\src\camera\camera_plugin_types.h %TARGET_DIR%\include\plugins\

@rem Copying AXIS plugin
set PLUGIN_NAME=axiscamplugin

mkdir %TARGET_DIR%\sample\%PLUGIN_NAME%\
mkdir %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
xcopy /Y ..\plugins\%PLUGIN_NAME%\src\*.txt %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
xcopy /Y ..\plugins\%PLUGIN_NAME%\src\*.h %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
xcopy /Y ..\plugins\%PLUGIN_NAME%\src\*.cpp %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
copy /Y ..\plugins\%PLUGIN_NAME%\Doxyfile %TARGET_DIR%\sample\%PLUGIN_NAME%\Doxyfile

@rem Removing unnecessary source files
del /Q /F %TARGET_DIR%\sample\%PLUGIN_NAME%\src\compatibility_info.cpp

@rem Generating documentation
set CUR_DIR_BAK=%~dp0
cd %TARGET_DIR%\sample\%PLUGIN_NAME%\
%environment%\bin\doxygen
cd %CUR_DIR_BAK%
del /F /Q %TARGET_DIR%\sample\%PLUGIN_NAME%\Doxyfile




@rem Copying image_library plugin
set PLUGIN_NAME=image_library_plugin

mkdir %TARGET_DIR%\sample\%PLUGIN_NAME%\
mkdir %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
xcopy /Y ..\plugins\%PLUGIN_NAME%\src\*.txt %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
xcopy /Y ..\plugins\%PLUGIN_NAME%\src\*.h %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
xcopy /Y ..\plugins\%PLUGIN_NAME%\src\*.cpp %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
copy /Y ..\plugins\%PLUGIN_NAME%\Doxyfile %TARGET_DIR%\sample\%PLUGIN_NAME%\Doxyfile
copy /Y ..\plugins\%PLUGIN_NAME%\readme.txt %TARGET_DIR%\sample\%PLUGIN_NAME%\readme.txt

@rem Removing unnecessary source files
del /Q /F %TARGET_DIR%\sample\%PLUGIN_NAME%\src\compatibility_info.cpp

@rem Generating documentation
set CUR_DIR_BAK=%~dp0
cd %TARGET_DIR%\sample\%PLUGIN_NAME%\
%environment%\bin\doxygen
cd %CUR_DIR_BAK%
del /F /Q %TARGET_DIR%\sample\%PLUGIN_NAME%\Doxyfile


@rem Copying rpi_cam plugin
set PLUGIN_NAME=rpi_cam

mkdir %TARGET_DIR%\sample\%PLUGIN_NAME%\
mkdir %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
xcopy /Y ..\plugins\%PLUGIN_NAME%\src\*.txt %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
xcopy /Y ..\plugins\%PLUGIN_NAME%\src\*.h %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
xcopy /Y ..\plugins\%PLUGIN_NAME%\src\*.cpp %TARGET_DIR%\sample\%PLUGIN_NAME%\src\
copy /Y ..\plugins\%PLUGIN_NAME%\Doxyfile %TARGET_DIR%\sample\%PLUGIN_NAME%\Doxyfile
copy /Y ..\plugins\%PLUGIN_NAME%\readme.txt %TARGET_DIR%\sample\%PLUGIN_NAME%\readme.txt

@rem Removing unnecessary source files
del /Q /F %TARGET_DIR%\sample\%PLUGIN_NAME%\src\compatibility_info.cpp

@rem Generating documentation
set CUR_DIR_BAK=%~dp0
cd %TARGET_DIR%\sample\%PLUGIN_NAME%\
%environment%\bin\doxygen
cd %CUR_DIR_BAK%
del /F /Q %TARGET_DIR%\sample\%PLUGIN_NAME%\Doxyfile
