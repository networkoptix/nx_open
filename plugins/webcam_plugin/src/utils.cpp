#include "utils.h"
#ifdef _WIN32
#include "dshow_utils.h"
#elif __linux__
#elif __APPLE__
#endif


namespace nx {
namespace webcam_plugin {
namespace utils {

std::vector<DeviceData> getDeviceList(
    bool getResolution, 
    nxcip::CompressionType targetCodecID)
{
    return
#ifdef _WIN32
        dshow::getDeviceList(getResolution, targetCodecID);
#elif __linux__
        //todo
        std::vector<DeviceInfo>();
#elif __APPLE__
        //todo
        std::vector<DeviceInfo>();
#endif
    std::vector<DeviceData>();
}

std::vector<nxcip::CompressionType> getSupportedCodecs(const char *devicePath)
{
    return
#ifdef _WIN32
        dshow::getSupportedCodecs(devicePath);
#elif __linux__
        //todo
        std::vector<DeviceInfo>();
#elif __APPLE__
        //todo
        std::vector<DeviceInfo>();
#endif
    std::vector<DeviceData>();
}

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    nxcip::CompressionType targetCodecID)
{
     return
#ifdef _WIN32
        dshow::getResolutionList(devicePath, targetCodecID);
#elif __linux__
        //todo
        std::vector<DeviceInfo>();
#elif __APPLE__
        //todo
        std::vector<DeviceInfo>();
#endif
    std::vector<nxcip::Resolution>();
}

} // namespace utils
} // namespace webcam_plugin
} // namespace nx