#ifdef __linux__
#pragma once

#include <camera/camera_plugin_types.h>
#include "camera/camera_plugin.h"

#include "device_data.h"

namespace nx {
namespace device {
namespace impl {

    /*!
     * get the name of the device reported by the underlying hardware.
     * @params[in]
     */
std::string getDeviceName(const char * devicePath);

/*!
* Get the list of devices on the system, with fields filled out.
*/
std::vector<DeviceData> getDeviceList();

/*!
* Get a list of codecs supported by this device
*/
std::vector<nxcip::CompressionType> getSupportedCodecs(const char * devicePath);

/*!
* Get the list of supported resolutions for the device with the given path.
* On Linux, this corresponds to the devices's /dev/video* entry.
* @param[in] getResolution - whether or not to fill each DeviceData with supported resolutions
* @param[in] codecID - the codec whose resolution list is desired
*/
std::vector<ResolutionData> getResolutionList(
    const char * devicePath,
    nxcip::CompressionType targetCodecID);

/*!
* Set the bitrate for the device with the given path.
* On Linux, this corresponds to the devices's /dev/video* entry.
* @param[in] bitrate - the bitrate to set in bits per second.
*/ 
void setBitrate(const char * devicePath, int bitrate, nxcip::CompressionType targetCodecID);

/*!
* Get the maximum bitrate supported by the camera
* @param[int] devicePath - the path to the device
*/ 
int getMaxBitrate(const char * devicePath, nxcip::CompressionType tagetCodecID));

} // namespace impl
} // namespace device
} // namespace nx

#endif