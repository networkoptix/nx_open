#pragma once

#include <vector>
#include <functional>

#include <QtCore/QString>

#include "camera_features.h"

namespace nx::vms_server_plugins::analytics::vivotek {

struct ObjectType
{
    QString nativeId;
    QString id;
    QString prettyName;
    std::function<bool(const CameraFeatures& features)> isSupported =
        [](const auto&) { return true; };
};

extern const std::vector<ObjectType> kObjectTypes;

} // namespace nx::vms_server_plugins::analytics::vivotek
