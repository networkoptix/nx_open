#pragma once

#include "device_info.h"

namespace utils{
namespace dshow{

    /*!
    * Get the list of devices on the system, with fields filled out.
    * @param[in] getResolution - whether or not to fill each DeviceInfo with supported resolutions
    */
    QList<DeviceInfo> getDeviceList(bool getResolution = false);

    /*!
    * Get the list of supported resolutions for the device with the given path.
    * On Windows this corresponds to the device's L"DevicePath" field for COM.
    * On Linux, this corresponds to the devices's /dev/video* entry.
    * ON Mac, tbd
    */
    QList<QSize> getResolutionList(const char * devicePath);

} // namespace dshow
} // namespace utils