#pragma once

#include <functional>
#include <vector>

#include <nx/sdk/analytics/rect.h>

#include <QtCore/QString>
#include <QtCore/QJsonObject>

#include "attributes.h"

namespace nx::vms_server_plugins::analytics::dahua {

struct ObjectType
{
    QString nativeId;
    QString id;
    QString prettyName;
    std::function<void(std::vector<Attribute>*, const QJsonObject&)> parseAttributes;

    static const ObjectType kUnknown;
    static const ObjectType kNonMotor;
    static const ObjectType kVehicle;
    static const ObjectType kPlate;
    static const ObjectType kHuman;
    static const ObjectType kHumanFace;

    static const ObjectType* findByNativeId(const QString& nativeId);
};

extern const std::vector<const ObjectType*>& kObjectTypes;

struct Object
{
    const ObjectType* type = nullptr;
    nx::sdk::analytics::Rect boundingBox;
    std::vector<Attribute> attributes;
};

} // namespace nx::vms_server_plugins::analytics::dahua
