#pragma once

#include <vector>
#include <functional>

#include <QtCore/QString>

#include "camera_settings.h"

namespace nx::vms_server_plugins::analytics::vivotek {

struct EventType
{
    QString nativeId;
    QString id;
    QString prettyName;
    bool isProlonged = false;
    std::function<bool(const CameraSettings& settings)> isAvailable =
        [](const auto&) { return true; };
};

extern const std::vector<EventType> kEventTypes;

} // namespace nx::vms_server_plugins::analytics::vivotek
