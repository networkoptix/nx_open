// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_color_type.h"

namespace nx::analytics::taxonomy {

QString AbstractColorType::idAsQString() const
{
    return QString::fromStdString(id());
}

QString AbstractColorType::nameAsQString() const
{
    return QString::fromStdString(name());
}

QStringList AbstractColorType::itemsAsQStringList() const
{
    QStringList result;
    for (const std::string& item: items())
        result.push_back(QString::fromStdString(item));
    return result;
}

QStringList AbstractColorType::ownItemsAsQStringList() const
{
    QStringList result;
    for (const std::string& item: ownItems())
        result.push_back(QString::fromStdString(item));
    return result;
}

QStringList AbstractColorType::baseItemsAsQStringList() const
{
    QStringList result;
    for (const std::string& item: baseItems())
        result.push_back(QString::fromStdString(item));
    return result;
}

} // namespace nx::analytics::taxonomy
