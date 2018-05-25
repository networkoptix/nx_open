#include "utils.h"
#ifdef _WIN32
#include "dshow_utils.h"
#elif __linux__
#include "v4l2_utils.h"
#elif __APPLE__
#endif


namespace nx {
namespace webcam_plugin {
namespace utils {

std::vector<DeviceData> getDeviceList()
{
    return
#ifdef _WIN32
        dshow::getDeviceList();
#elif __linux__
        //todo
        v4l2::getDeviceList();
#elif __APPLE__
        //todo
        std::vector<DeviceData>();
#endif
    std::vector<DeviceData>();
}

std::vector<nxcip::CompressionType> getSupportedCodecs(const char *devicePath)
{
    return
#ifdef _WIN32
        dshow::getSupportedCodecs(devicePath);
#elif __linux__
        v4l2::getSupportedCodecs(devicePath);
#elif __APPLE__
        //todo
        std::vector<nxcip>();
#endif
    std::vector<nxcip::CompressionType>();
}

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    nxcip::CompressionType targetCodecID)
{
     return
#ifdef _WIN32
        dshow::getResolutionList(devicePath, targetCodecID);
#elif __linux__
        v4l2::getResolutionList(devicePath, targetCodecID);
#elif __APPLE__
        //todo
        std::vector<DeviceInfo>();
#endif
    std::vector<ResolutionData>();
}

} // namespace utils
} // namespace webcam_plugin
} // namespace nx