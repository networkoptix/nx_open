// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "structures.h"

namespace nx::vms::client::core::event_search {

Item Item::create(std::chrono::milliseconds timestamp, const QString& name)
{
    return { nx::Uuid::createUuid(), name, timestamp};
}

QString Item::toString() const
{
    return name.isEmpty()
        ? QString::number(timestamp.count())
        : QString("%1 %2").arg(QString::number(timestamp.count()), name);
}

nx::Uuid Accessor::id(const Type& item)
{
    return item.id;
}

Accessor::TimeType Accessor::startTime(const Type& item)
{
    return item.timestamp;
}

} // namespace nx::vms::client::core::event_search
