#ifdef __linux__
#pragma once

#include <memory>
#include <linux/videodev2.h>

#include <camera/camera_plugin_types.h>
#include "camera/camera_plugin.h"

#include "device/device_data.h"
#include "device/abstract_compression_type_descriptor.h"

namespace nx {
namespace usb_cam {
namespace device {
namespace impl {

class V4L2CompressionTypeDescriptor : public AbstractCompressionTypeDescriptor
{
public:
    V4L2CompressionTypeDescriptor(const struct v4l2_fmtdesc& formatEnum);
    ~V4L2CompressionTypeDescriptor();
    struct v4l2_fmtdesc * descriptor();
    __u32 pixelFormat()const;
    virtual nxcip::CompressionType toNxCompressionType() const override;
private:
    struct v4l2_fmtdesc * m_descriptor;
};

/**
* get the name of the device reported by the underlying hardware.
* @params[in]
*/
std::string getDeviceName(const char * devicePath);

/**
* Get the list of devices on the system, with fields filled out.
*/
std::vector<DeviceData> getDeviceList();

/**
* Get a list of codecs supported by this device
*/
std::vector<device::CompressionTypeDescriptorPtr> getSupportedCodecs(const char * devicePath);

/**
* Get the list of supported resolutions for the device with the given path.
* On Linux, this corresponds to the devices's /dev/video* entry.
* @param[in] getResolution - whether or not to fill each DeviceData with supported resolutions
* @param[in] codecId - the codec whose resolution list is desired
*/
std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    const device::CompressionTypeDescriptorPtr& targetCodecID);

/**
* Set the bitrate for the device with the given path.
* On Linux, this corresponds to the devices's /dev/video* entry.
* @param[in] bitrate - the bitrate to set in bits per second.
*/ 
void setBitrate(const char * devicePath, int bitrate, const device::CompressionTypeDescriptorPtr& targetCodecID);

/**
* Get the maximum bitrate supported by the camera
* @param[int] devicePath - the path to the device
*/ 
int getMaxBitrate(const char * devicePath, const device::CompressionTypeDescriptorPtr& tagetCodecID);

} // namespace impl
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif