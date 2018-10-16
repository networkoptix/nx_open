#ifdef _WIN32
#include "dshow/dshow_utils.h"
#elif __linux__
#include "v4l2/v4l2_utils.h"
#endif

#include "utils.h"
#include "rpi/rpi_utils.h"

namespace nx {
namespace usb_cam {
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

    if(rpi::isMmal(getDeviceName(devicePath)))
        return rpi::mmalResolutionList();

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
    if(targetCodecID && rpi::isMmal(getDeviceName(devicePath)))
        impl::setBitrate(devicePath, bitrate, targetCodecID);
}

int getMaxBitrate(const char * devicePath, const device::CompressionTypeDescriptorPtr& targetCodecID)
{
    if(!targetCodecID)
        return 0;
    if(rpi::isMmal(getDeviceName(devicePath)))
        return rpi::mmalMaxBitrate();

    // usb_cameras don't support setting their bitrate. only the rpi mmal camera does.
#if 0    
    return impl::getMaxBitrate(devicePath, targetCodecID);
#else
    return 0;
#endif
}

} // namespace device
} // namespace usb_cam
} // namespace nx
