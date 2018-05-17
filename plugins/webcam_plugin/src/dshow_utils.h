#pragma once

#include "device_data.h"

namespace nx {
namespace webcam_plugin {
namespace utils {
namespace dshow {

    /*!
    * Get the list of devices on the system, with fields filled out.
    * @param[in] getResolution - whether or not to fill each DeviceData with supported resolutions
    */
    QList<DeviceData> getDeviceList(bool getResolution = false);

    /*!
    * Get the list of supported resolutions for the device with the given path.
    * On Windows this corresponds to the device's L"DevicePath" field for COM.
    * On Linux, this corresponds to the devices's /dev/video* entry.
    * ON Mac, tbd
    */
    QList<ResolutionData> getResolutionList(const char * devicePath);

} // namespace dshow
} // namespace utils
} // namespace webcam_plugin
} // namespace nx
