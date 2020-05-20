#pragma once

#include <nx/utils/url.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class CameraFeatures
{
public:
    struct Vca
    {
        bool crowd = false;
        bool loitering = false;
        bool intrusion = false;
        bool lineCrossing = false;
        bool missingObject = false;
        bool unattendedObject = false;
        bool face = false;
    };
    std::optional<Vca> vca;

public:
    static CameraFeatures fetchFrom(const nx::utils::Url& cameraUrl);
};

} // namespace nx::vms_server_plugins::analytics::vivotek
