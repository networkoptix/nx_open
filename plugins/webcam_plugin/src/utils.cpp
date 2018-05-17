#include "utils.h"
#ifdef _WIN32
#include "dshow_utils.h"
#elif __linux__
#elif __APPLE__
#endif


namespace nx {
namespace webcam_plugin {
namespace utils {

QList<DeviceData> getDeviceList(bool getResolution)
{
    return
#ifdef _WIN32
        dshow::getDeviceList(getResolution);
#elif __linux__
        //todo
        QList<DeviceInfo>();
#elif __APPLE__
        //todo
        QList<DeviceInfo>();
#endif
    QList<DeviceData>();
}

QList<ResolutionData> getResolutionList(const char * devicePath)
{
     return
#ifdef _WIN32
        dshow::getResolutionList(devicePath);
#elif __linux__
        //todo
        QList<DeviceInfo>();
#elif __APPLE__
        //todo
        QList<DeviceInfo>();
#endif
    QList<QSize>();
}

} // namespace utils
} // namespace webcam_plugin
} // namespace nx