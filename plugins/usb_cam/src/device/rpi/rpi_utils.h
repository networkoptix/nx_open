#include <string>

#include <nx/utils/app_info.h>

#include "device/device_data.h"

namespace nx {
namespace usb_cam {
namespace device {
namespace rpi {

/**
 * Get a list of devices of the form /dev/video* available on the RaspberryPi.
 * Note that it is not the same path as Desktop Linux, which uses /dev/v4l/by-id/*. 
 * Raspberry Pi does not create such a directory for its integrated camera, 
 * so instead /dev/video* is searched.
 */
std::vector<std::string> getRpiDevicePaths();

/**
 * Return the resolution list for the rpi integrated camera.
 */
std::vector<device::ResolutionData> mmalResolutionList();

/**
 * Return the maximum bitrate of the rpi integrated camera.
 */
int mmalMaxBitrate();

/**
 * Returns true if the device is the rpi and the name of the camera contains "mmal".
 */
bool isMmal(const std::string& deviceName);

/**
 * Returns true if the plugin is running on the rpi.
 */
bool isRpi();

} // namespace rpi
} // namespace device
} // namespace usb_cam
} // namespace nx