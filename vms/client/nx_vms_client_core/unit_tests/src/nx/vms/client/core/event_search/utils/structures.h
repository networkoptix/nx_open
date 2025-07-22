// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/utils/uuid.h>
#include <nx/utils/log/format.h>

namespace nx::vms::client::core::event_search {

/** Test item with basic fields for the test purposes only. */
struct Item
{
    QString id;
    std::chrono::milliseconds timestamp;
    QString name;

    static Item create(std::chrono::milliseconds timestamp, const QString& name = {});
    static Item create(
        const QString& id, std::chrono::milliseconds timestamp, const QString& name = {});

    QString toString() const;

    bool operator==(const Item&) const = default;
    bool operator!=(const Item&) const = default;
};

struct Accessor
{
    using Type = Item;
    using TimeType = std::chrono::milliseconds;
    using IdType = QString;

    static QString id(const Type& item);

    static TimeType startTime(const Type& item);

    static bool equal(const Item& a, const Item& b);
};

inline void PrintTo(const Item& item, ::std::ostream* os)
{
    *os << nx::format("{id: %1, timestamp: %2, name: %3}",
        item.id, item.timestamp.count(), item.name).toStdString();
}

} // namespace nx::vms::client::core::event_search
