// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <nx/utils/uuid.h>

namespace nx::vms::client::core::event_search {

/** Test item with basic fields for the test purposes only. */
struct Item
{
    nx::Uuid id;
    QString name;
    std::chrono::milliseconds timestamp;

    static Item create(std::chrono::milliseconds timestamp, const QString& name = {});
    QString toString() const;
};

struct Accessor
{
    using Type = Item;
    using TimeType = std::chrono::milliseconds;

    static nx::Uuid id(const Type& item);

    static TimeType startTime(const Type& item);
};

} // namespace nx::vms::client::core::event_search
