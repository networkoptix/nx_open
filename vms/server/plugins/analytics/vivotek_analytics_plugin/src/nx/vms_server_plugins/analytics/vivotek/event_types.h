#pragma once

#include <vector>
#include <functional>

#include <QtCore/QString>

#include "camera_features.h"

namespace nx::vms_server_plugins::analytics::vivotek {

struct EventType
{
    QString nativeId;
    QString id;
    QString prettyName;
    bool isProlonged = false;
    std::function<bool(const CameraFeatures& features)> isSupported =
        [](const auto&) { return true; };
};

extern const std::vector<EventType> kEventTypes;

} // namespace nx::vms_server_plugins::analytics::vivotek
