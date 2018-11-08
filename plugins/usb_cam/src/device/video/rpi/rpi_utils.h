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
 * Return the resolution list for the rpi integrated camera.
 */
std::vector<device::video::ResolutionData> getMmalResolutionList();

/**
 * Return the maximum bitrate of the rpi integrated camera.
 */
int getMmalMaxBitrate();

/**
 * Returns true if the mediaserver is running on the rpi and the name of the camera
 *    contains "mmal".
 */
bool isMmalCamera(const std::string& deviceName);

/**
 * Returns true if the plugin is running on the rpi.
 */
bool isRpi();

} // namespace rpi
} // namespace video
} // namespace device
} // namespace usb_cam
} // namespace nx
