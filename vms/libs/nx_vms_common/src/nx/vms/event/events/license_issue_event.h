// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/reasoned_event.h>

namespace nx::vms::event {

class NX_VMS_COMMON_API LicenseIssueEvent: public ReasonedEvent
{
    using base_type = ReasonedEvent;

public:
    explicit LicenseIssueEvent(
        const QnResourcePtr& resource,
        qint64 timeStamp,
        EventReason reasonCode,
        const UuidSet& disabledCameras);

    static QString encodeCameras(const UuidSet& cameras);
    static UuidSet decodeCameras(const EventParameters& params);
};

} // namespace nx::vms::event
