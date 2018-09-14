#pragma once

#include <memory>

#include <camera/camera_plugin_types.h>
#include "camera/camera_plugin.h"

#include "device_data.h"

namespace nx {
namespace usb_cam {
namespace device {


std::string getDeviceName(const char * devicePath);

/*!
* Get the list of devices on the system, with fields filled out.
* @param[in] getResolution - whether or not to fill each DeviceData with supported resolutions
* @param[in] codecID - the codec whose resolution list is desired
*/
std::vector<DeviceData> getDeviceList();

/*!
* Get a list of codecs supported by this device
*/
std::vector<device::CompressionTypeDescriptorPtr> getSupportedCodecs(const char * devicePath);

/*!
* Get the list of supported resolutions for the device with the given path.
* On Windows this corresponds to the device's L"DevicePath" field for COM.
* On Linux, this corresponds to the devices's /dev/video* entry.
* @param[in] devicePath - the path to the device.
* @param[in] targetCodecID - the codec whose resolution list is desired
*/
std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    const device::CompressionTypeDescriptorPtr& targetCodecID);

/*!
* Set the bitrate for the device with the given \a devicePath.
* On Windows this corresponds to the device's L"DevicePath" field for COM.
* On Linux, this corresponds to the devices's /dev/video* entry.
* @param[int] devicePath - the path to the device
* @param[in] targetCodecID - the target codec to set bitrate for
*/
void setBitrate(
    const char * devicePath,
    int bitrate,
    const device::CompressionTypeDescriptorPtr& targetCodecID);

/*!
* Get the maximum bitrate supported by the camera
* @param[int] devicePath - the path to the device
* * @param[in] targetCodecID - the target codec to get bitrate for
*/ 
int getMaxBitrate(
    const char * devicePath,
    const device::CompressionTypeDescriptorPtr& targetCodecID);

} // namespace device
} // namespace usb_cam
} // namespace nx