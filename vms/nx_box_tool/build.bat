FOR /F "tokens=*" %%i in ('type build_options.conf') do SET %%i

set PATH=%DEV_PATH%\python;%DEV_PATH%\python\Scripts;%PATH%
set PYTHONPATH=%DEV_PATH%\python\lib\python3.7\site-packages

pyinstaller ^
    -y ^
    -F --hidden-import json ^
    --distpath %BUILD_DIR%\dist ^
    --workpath %BUILD_DIR%\build ^
    --specpath %BUILD_DIR% ^
    --add-binary "%DEV_PATH%\python\Lib\site-packages\cv2\opencv_ffmpeg400_64.dll;." ^
    -p %SRC_DIR%\lib ^
    %SRC_DIR%\bin\nx_box_tool
