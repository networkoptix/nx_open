// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "structures.h"

namespace nx::vms::client::core::event_search {

Item Item::create(std::chrono::milliseconds timestamp, const QString& name)
{
    return create(nx::Uuid::createUuid().toSimpleString(), timestamp, name);
}

Item Item::create(const QString& id, std::chrono::milliseconds timestamp, const QString& name)
{
    return Item{.id = id, .timestamp = timestamp, .name = name};
}

QString Item::toString() const
{
    return name.isEmpty()
        ? QString::number(timestamp.count())
        : QString("%1 %2").arg(QString::number(timestamp.count()), name);
}

QString Accessor::id(const Type& item)
{
    return item.id;
}

Accessor::TimeType Accessor::startTime(const Type& item)
{
    return item.timestamp;
}

bool Accessor::equal(const Item& a, const Item& b)
{
    return a == b;
}

} // namespace nx::vms::client::core::event_search
