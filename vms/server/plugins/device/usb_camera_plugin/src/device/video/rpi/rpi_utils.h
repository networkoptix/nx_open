#ifdef __linux__

#pragma once

#include <string>
#include <vector>

#include "device/video/resolution_data.h"

namespace nx {
namespace usb_cam {
namespace device {
namespace video {
namespace rpi {

/**
 * Get the unique id of the rpi integrated camera (the "Serial" value found in /proc/cpuinfo).
 */
std::string getMmalUniqueId();

/**
 * Get the resolution list for the rpi integrated camera.
 */
std::vector<device::video::ResolutionData> getMmalResolutionList();

/**
 * Get the maximum bitrate of the rpi integrated camera.
 */
int getMmalMaxBitrate();

/**
 * Returns true if the name of the camera contains "mmal".
 */
bool isMmalCamera(const std::string& deviceName);

} // namespace rpi
} // namespace video
} // namespace device
} // namespace usb_cam
} // namespace nx


#endif //__linux__
