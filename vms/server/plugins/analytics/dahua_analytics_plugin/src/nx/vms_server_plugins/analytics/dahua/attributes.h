#pragma once

#include <utility>

#include <nx/sdk/helpers/attribute.h>

#include <QtCore/QString>

namespace nx::vms_server_plugins::analytics::dahua {

struct Attribute
{
    using Type = nx::sdk::Attribute::Type;

    QString name;
    QString value;
    Type type = Type::undefined;

    Attribute(QString name, QString value, Type type = Type::undefined):
        name(std::move(name)),
        value(std::move(value)),
        type(type)
    {
    }
};

} // namespace nx::vms_server_plugins::analytics::dahua
