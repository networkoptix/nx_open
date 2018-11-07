#ifdef __linux__
#pragma once

#include <string>

#include "device/device_data.h"

namespace nx {
namespace usb_cam {
namespace device {
namespace rpi {

/**
 * Return the resolution list for the rpi integrated camera.
 */
std::vector<device::ResolutionData> getMmalResolutionList();

/**
 * Return the maximum bitrate of the rpi integrated camera.
 */
int getMmalMaxBitrate();

/**
 * Returns true if the mediaserver is running on the rpi and the name of the camera contains "mmal".
 */
bool isMmalCamera(const std::string& deviceName);

/**
 * Returns true if the plugin is running on the rpi.
 */
bool isRpi();

} // namespace rpi
} // namespace device
} // namespace usb_cam
} // namespace nx

#endif