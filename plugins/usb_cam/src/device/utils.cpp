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

std::vector<device::CompressionTypeDescriptorPtr> getSupportedCodecs(const char *devicePath)
{
    return impl::getSupportedCodecs(devicePath);
}

std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    const device::CompressionTypeDescriptorPtr& targetCodecID)
{
    if (!targetCodecID)
        return {};

    auto list = impl::getResolutionList(devicePath, targetCodecID);
    std::sort(list.begin(), list.end(),
        [](const ResolutionData& a, const ResolutionData& b)
        {
            return a.width * a.height < b.width * b.height;
        });
    return list;
}

void setBitrate(const char * devicePath, int bitrate, const device::CompressionTypeDescriptorPtr& targetCodecID)
{
    if(targetCodecID)
        impl::setBitrate(devicePath, bitrate, targetCodecID);
}

int getMaxBitrate(const char * devicePath, const device::CompressionTypeDescriptorPtr& targetCodecID)
{
    if(!targetCodecID)
        return 10000000;
    return impl::getMaxBitrate(devicePath, targetCodecID);
}

} // namespace device
} // namespace nx
