#pragma once

#include <functional>
#include <vector>

#include <nx/sdk/helpers/attribute.h>
#include <nx/sdk/analytics/rect.h>

#include <QtCore/QString>
#include <QtCore/QJsonObject>

namespace nx::vms_server_plugins::analytics::dahua {

struct ObjectAttribute
{
    QString name;
    QString value;
    nx::sdk::Attribute::Type type = nx::sdk::Attribute::Type::undefined;
};

struct ObjectType
{
    QString nativeId;
    QString id;
    QString prettyName;
    std::function<std::vector<ObjectAttribute>(const QJsonObject& object)> parseAttributes;

    static const ObjectType kUnknown;
    static const ObjectType kNonMotor;
    static const ObjectType kVehicle;
    static const ObjectType kHuman;
    static const ObjectType kHumanFace;

    static const ObjectType* findByNativeId(const QString& nativeId);
};

extern const std::vector<const ObjectType*>& kObjectTypes;

struct Object
{
    const ObjectType* type = nullptr;
    nx::sdk::analytics::Rect boundingBox;
    std::vector<ObjectAttribute> attributes;
};

} // namespace nx::vms_server_plugins::analytics::dahua
