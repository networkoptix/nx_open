#include "utils.h"

#ifdef _WIN32
#include "windows/dshow_utils.h"
#else // __linux__
#include "linux/alsa_utils.h"
#endif

namespace nx {
namespace usb_cam {
namespace device {
namespace audio {

void fillCameraAuxiliaryData(nxcip::CameraInfo* cameras, int cameraCount)
{
    detail::fillCameraAuxiliaryData(cameras, cameraCount);
}

bool pluggedIn(const std::string& devicePath)
{
    return detail::pluggedIn(devicePath);
}

} // namespace audio
} // namespace device
} // namespace usb_cam
} // namespace nx