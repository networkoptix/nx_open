#ifdef _WIN32
#include "dshow/dshow_utils.h"
#elif __linux__
#include "v4l2/v4l2_utils.h"
#endif

#include <nx/utils/app_info.h>
#include "utils.h"

namespace nx {
namespace device {
std::string getDeviceName(const char * devicePath)
{
    return impl::getDeviceName(devicePath);
}

std::vector<DeviceData> getDeviceList()
{
    return impl::getDeviceList(); 
}

std::vector<std::shared_ptr<AbstractCompressionTypeDescriptor>> getSupportedCodecs(const char *devicePath)
{
    return impl::getSupportedCodecs(devicePath);
}

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    const std::shared_ptr<AbstractCompressionTypeDescriptor>& targetCodecID)
{
    return impl::getResolutionList(devicePath, targetCodecID);
}

void setBitrate(const char * devicePath, int bitrate, nxcip::CompressionType targetCodecID)
{
    return impl::setBitrate(devicePath, bitrate, targetCodecID);
}

int getMaxBitrate(const char * devicePath, nxcip::CompressionType targetCodecID)
{
    return impl::getMaxBitrate(devicePath, targetCodecID);
}

} // namespace device
} // namespace nx
