#pragma once

#include <memory>
#include <vector>
#include <string>

#include <camera/camera_plugin.h>
#include <camera/camera_plugin_types.h>

#include "resolution_data.h"
#include "device/device_data.h"
#include "device/abstract_compression_type_descriptor.h"

namespace nx {
namespace usb_cam {
namespace device {
namespace video {

/**
 * Get the gui friendly name of the device.
 */
std::string getDeviceName(const std::string& devicePath);

/**
* Get the list of devices on the system, with fields filled out.
* @param[in] getResolution - whether or not to fill each DeviceData with supported resolutions
* @param[in] codecId - the codec whose resolution list is desired
*/
std::vector<DeviceData> getDeviceList();

/**
* Get a list of codecs supported by this device
*/
std::vector<device::CompressionTypeDescriptorPtr> getSupportedCodecs(const std::string& devicePath);

/**
* Get the list of supported resolutions for the device with the given path.
* On Windows this corresponds to the device's L"DevicePath" field for COM.
* On Linux, this corresponds to the devices's /dev/video* entry.
* @param[in] devicePath - the path to the device.
* @param[in] targetCodecID - the codec whose resolution list is desired
*/
std::vector<ResolutionData> getResolutionList(
    const std::string& devicePath,
    const device::CompressionTypeDescriptorPtr& targetCodecID);

/**
* Set the bitrate for the device with the given \a devicePath.
* On Windows this corresponds to the device's L"DevicePath" field for COM.
* On Linux, this corresponds to the devices's /dev/video* entry.
* @param[int] devicePath - the path to the device
* @param[in] targetCodecID - the target codec to set bitrate for
*/
void setBitrate(
    const std::string& devicePath,
    int bitrate,
    const device::CompressionTypeDescriptorPtr& targetCodecID);

/**
* Get the maximum bitrate supported by the camera
* @param[int] devicePath - the path to the device
* * @param[in] targetCodecID - the target codec to get bitrate for
*/ 
int getMaxBitrate(
    const std::string& devicePath,
    const device::CompressionTypeDescriptorPtr& targetCodecID);

} // namespace video
} // namespace device
} // namespace usb_cam
} // namespace nx