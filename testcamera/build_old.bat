call "%VS90COMNTOOLS%vsvars32.bat"
call qtvars.bat

MSBuild src\testcamera.vcproj /t:Build /p:Configuration=Debug