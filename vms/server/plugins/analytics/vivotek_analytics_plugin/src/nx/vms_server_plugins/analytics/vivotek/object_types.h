#pragma once

#include <vector>
#include <functional>

#include <QtCore/QString>

#include "camera_settings.h"

namespace nx::vms_server_plugins::analytics::vivotek {

struct ObjectType
{
    QString nativeId;
    QString id;
    QString prettyName;
    std::function<bool(const CameraSettings& settings)> isAvailable =
        [](const auto&) { return true; };
};

extern const std::vector<ObjectType> kObjectTypes;

} // namespace nx::vms_server_plugins::analytics::vivotek
