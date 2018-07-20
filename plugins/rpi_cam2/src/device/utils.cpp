#include "v4l2_utils.h"


namespace nx {
namespace device {
namespace utils {

std::vector<DeviceData> getDeviceList()
{
    return v4l2::getDeviceList();
}

std::vector<nxcip::CompressionType> getSupportedCodecs(const char *devicePath)
{
    return v4l2::getSupportedCodecs(devicePath);
}

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    nxcip::CompressionType targetCodecID)
{
    std::vector<ResolutionData> list;

    list.push_back(ResolutionData(1920, 1080, 30));
    list.push_back(ResolutionData(1280, 720, 30));
    list.push_back(ResolutionData(800, 600, 30));
    list.push_back(ResolutionData(640, 480, 30));
    list.push_back(ResolutionData(640, 360, 30));
    list.push_back(ResolutionData(480, 270, 30));
    list.push_back(ResolutionData(320, 180, 30));

    return list;

     //return v4l2::getResolutionList(devicePath, targetCodecID);
}

void setBitrate(const char * devicePath, int bitrate)
{
    return v4l2::setBitrate(devicePath, bitrate);
}


} // namespace utils
} // namespace device
} // namespace nx