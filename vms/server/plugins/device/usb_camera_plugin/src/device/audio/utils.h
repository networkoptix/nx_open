#pragma once

#include <string>
#include <vector>

#include "device/device_data.h"

namespace nx::usb_cam::device::audio {

void selectAudioDevices(std::vector<DeviceData>& devices);
bool pluggedIn(const std::string& devicePath);
void uninitialize();

} // namespace nx::usb_cam::device::audio
