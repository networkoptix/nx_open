#include "utils.h"

#ifdef _WIN32
#include "windows/dshow_utils.h"
#else // __linux__
#include "linux/alsa_utils.h"
#endif

namespace nx::usb_cam::device::audio {

void selectAudioDevices(std::vector<DeviceData>& devices)
{
    detail::selectAudioDevices(devices);
}

bool pluggedIn(const std::string& devicePath)
{
    return detail::pluggedIn(devicePath);
}

void uninitialize()
{
    detail::uninitialize();
}

} // namespace nx::usb_cam::device::audio
