for /F "tokens=*" %%i in ('type build_options.conf') do set %%i

set PATH=%DEV_PATH%\python;%PATH%
set PYTHONPATH=%DEV_PATH%\python\lib\python3.7\site-packages

python3.exe -m PyInstaller ^
    -y ^
    -F --hidden-import json ^
    --distpath %BUILD_DIR%\dist ^
    --workpath %BUILD_DIR%\build ^
    --specpath %BUILD_DIR% ^
    --add-binary "%DEV_PATH%\python\Lib\site-packages\cv2\opencv_ffmpeg400_64.dll;." ^
    -p %SRC_DIR%\lib ^
    %SRC_DIR%\bin\vms_benchmark
