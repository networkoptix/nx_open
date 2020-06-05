#pragma once

#include <nx/utils/url.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class CameraFeatures
{
public:
    struct Vca
    {
        bool crowdDetection = false;
        bool loiteringDetection = false;
        bool intrusionDetection = false;
        bool lineCrossingDetection = false;
        bool missingObjectDetection = false;
        bool unattendedObjectDetection = false;
        bool faceDetection = false;
        bool runningDetection = false;
    };
    std::optional<Vca> vca;

public:
    static CameraFeatures fetchFrom(const nx::utils::Url& cameraUrl);
};

} // namespace nx::vms_server_plugins::analytics::vivotek
