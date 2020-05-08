#pragma once

#include <nx/utils/url.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class CameraFeatures
{
public:
    bool vca = false;

public:
    static CameraFeatures fetchFrom(const nx::utils::Url& cameraUrl);
};

} // namespace nx::vms_server_plugins::analytics::vivotek
