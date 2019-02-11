#include "utils.h"

#ifdef _WIN32
#include "windows/dshow_utils.h"
#else // __linux__
#include "linux/alsa_utils.h"
#endif

namespace nx::usb_cam::device::audio {

void fillCameraAuxiliaryData(nxcip::CameraInfo* cameras, int cameraCount)
{
    detail::fillCameraAuxiliaryData(cameras, cameraCount);
}

bool pluggedIn(const std::string& devicePath)
{
    return detail::pluggedIn(devicePath);
}

} // namespace nx::usb_cam::device::audio
