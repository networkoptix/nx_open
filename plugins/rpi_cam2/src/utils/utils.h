#pragma once

#include "device_data.h"

namespace nx {
namespace rpi_cam2 {
namespace utils  {

#define debug(...) fprintf(stderr, "[rpi_cam2] " __VA_ARGS__)

/*!
* Get the list of devices on the system, with fields filled out.
* @param[in] getResolution - whether or not to fill each DeviceData with supported resolutions
* @param[in] codecID - the codec whose resolution list is desired
*/
std::vector<DeviceData> getDeviceList();

/*!
* Get a list of codecs supported by this device
*/
std::vector<nxcip::CompressionType> getSupportedCodecs(const char * devicePath);

/*!
* Get the list of supported resolutions for the device with the given path.
* On Windows this corresponds to the device's L"DevicePath" field for COM.
* On Linux, this corresponds to the devices's /dev/video* entry.
* ON Mac, tbd
* @param[in] getResolution - whether or not to fill each DeviceData with supported resolutions
* @param[in] codecID - the codec whose resolution list is desired
*/
std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    nxcip::CompressionType targetCodecID);

} // namespace utils
} // namespace rpi_cam2
} // namespace nx