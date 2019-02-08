#include "utils.h"

#include <algorithm>

#ifdef _WIN32
#include "windows/dshow_utils.h"
#elif __linux__
#include "linux/v4l2_utils.h"
#endif

namespace nx {
namespace usb_cam {
namespace device {
namespace video {

std::string getDeviceName(const std::string& devicePath)
{
    return detail::getDeviceName(devicePath);
}

std::vector<DeviceData> getDeviceList()
{
    return detail::getDeviceList();
}

std::vector<device::CompressionTypeDescriptorPtr> getSupportedCodecs(const std::string& devicePath)
{
    return detail::getSupportedCodecs(devicePath);
}

std::vector<ResolutionData> getResolutionList(
    const std::string& devicePath,
    const device::CompressionTypeDescriptorPtr& targetCodecID)
{
    if (!targetCodecID)
        return {};

    auto list = detail::getResolutionList(devicePath, targetCodecID);
    std::sort(list.begin(), list.end(),
        [](const ResolutionData& a, const ResolutionData& b)
        {
            return a.width * a.height < b.width * b.height;
        });
    return list;
}

void setBitrate(
    const std::string& devicePath,
    int bitrate,
    const device::CompressionTypeDescriptorPtr& targetCodecID)
{
    if(targetCodecID)
        detail::setBitrate(devicePath, bitrate, targetCodecID);
}

int getMaxBitrate(
    const std::string& devicePath,
    const device::CompressionTypeDescriptorPtr& targetCodecID)
{
    if(!targetCodecID)
        return 0;

    return detail::getMaxBitrate(devicePath, targetCodecID);
}

} // namespace video
} // namespace device
} // namespace usb_cam
} // namespace nx
