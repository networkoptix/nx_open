// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_enum_type.h"

namespace nx::analytics::taxonomy {

QString AbstractEnumType::idAsQString() const
{
    return QString::fromStdString(id());
}

QString AbstractEnumType::nameAsQString() const
{
    return QString::fromStdString(name());
}

QStringList AbstractEnumType::ownItemsAsQStringList() const
{
    QStringList result;
    for (const std::string& item: ownItems())
        result.push_back(QString::fromStdString(item));
    return result;
}

QStringList AbstractEnumType::itemsAsQStringList() const
{
    QStringList result;
    for (const std::string& item: items())
        result.push_back(QString::fromStdString(item));
    return result;
}

} // namespace nx::analytics::taxonomy
