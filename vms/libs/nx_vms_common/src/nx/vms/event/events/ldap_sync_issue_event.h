// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/reasoned_event.h>

namespace nx::vms::event {

class AggregationInfo;

class NX_VMS_COMMON_API LdapSyncIssueEvent: public ReasonedEvent
{
    using base_type = ReasonedEvent;

public:
    LdapSyncIssueEvent(
        vms::api::EventReason reason,
        std::chrono::microseconds timestamp,
        std::optional<std::chrono::seconds> syncInterval);

    virtual EventParameters getRuntimeParams() const override;

    static bool isValidReason(vms::api::EventReason reason);

    static bool isAggregatedEvent(const EventParameters& params);

    using Reasons = std::map<vms::api::EventReason, int>;
    static Reasons decodeReasons(const EventParameters& params);
    static void encodeReasons(const AggregationInfo& info, EventParameters& target);

    static std::optional<std::chrono::seconds> syncInterval(const EventParameters& params);

private:
    std::optional<std::chrono::seconds> m_syncInterval;
};

} // namespace nx::vms::event
