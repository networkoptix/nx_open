#ifdef _WIN32
#include "dshow.h"
#elif __linux__
#include "v4l2.h"
#endif

#include <nx/utils/app_info.h>

namespace nx {
namespace device {

std::vector<DeviceData> getDeviceList()
{
    return impl::getDeviceList(); 
}

std::vector<nxcip::CompressionType> getSupportedCodecs(const char *devicePath)
{
    return impl::getSupportedCodecs(devicePath);
}

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    nxcip::CompressionType targetCodecID)
{
    return impl::getResolutionList(devicePath, targetCodecID);
}

void setBitrate(const char * devicePath, int bitrate)
{
    return impl::setBitrate(devicePath, bitrate);
}

int getMaxBitrate(const char * devicePath)
{
    return impl::getMaxBitrate(devicePath);
}

} // namespace device
} // namespace nx
