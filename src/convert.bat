del release /Q
del debug /Q

call version.bat

echo #ifndef UNIVERSAL_CLIENT_VERSION_H_ > version.h
echo #define UNIVERSAL_CLIENT_VERSION_H_ >> version.h
echo // This file is generated. Go to version.bat >> version.h
echo static const char* ORGANIZATION_NAME="%ORGANIZATION_NAME%"; >> version.h
echo static const char* APPLICATION_NAME="%APPLICATION_NAME%"; >> version.h
echo static const char* APPLICATION_VERSION="%APPLICATION_VERSION%"; >> version.h
echo #endif // UNIVERSAL_CLIENT_VERSION_H_ >> version.h

pro_helper.exe uniclient.pro const.pro _report.txt
call convert_qt_to_vs.bat uniclient


