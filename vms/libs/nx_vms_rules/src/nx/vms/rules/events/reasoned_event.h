// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ReasonedEvent: public BasicEvent
{
    Q_OBJECT
    using base_type = BasicEvent;

public:
    using EventReason = nx::vms::api::EventReason;

    FIELD(QnUuid, serverId, setServerId)
    FIELD(nx::vms::api::EventReason, reasonCode, setReasonCode)
    FIELD(QString, reasonText, setReasonText)

public:
    ReasonedEvent() = default;
    ReasonedEvent(
        std::chrono::microseconds timestamp,
        QnUuid serverId,
        EventReason reasonCode,
        const QString& reasonText);

    virtual QString uniqueName() const override;
    virtual QString resourceKey() const override;

protected:
    ~ReasonedEvent() = default; //< Intended for use as base class only.
};

} // namespace nx::vms::rules
