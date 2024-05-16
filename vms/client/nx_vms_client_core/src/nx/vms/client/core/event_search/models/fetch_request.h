// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <recording/time_period.h>
#include <nx/vms/client/core/event_search/event_search_globals.h>

namespace nx::vms::client::core {

struct NX_VMS_CLIENT_CORE_API FetchRequest
{
    Q_GADGET

    Q_PROPERTY(EventSearch::FetchDirection direction
        MEMBER direction)
    Q_PROPERTY(std::chrono::microseconds centralPointUs
        MEMBER centralPointUs)

public:
    static void registerQmlType();

    EventSearch::FetchDirection direction = EventSearch::FetchDirection::older;
    std::chrono::microseconds centralPointUs;

    QnTimePeriod period(const OptionalTimePeriod& interestPeriod) const;
};

using OptionalFetchRequest = std::optional<FetchRequest>;

} // namespace nx::vms::client::core
