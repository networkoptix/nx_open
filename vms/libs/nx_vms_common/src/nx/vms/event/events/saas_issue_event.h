// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/reasoned_event.h>

namespace nx::vms::event {

class NX_VMS_COMMON_API SaasIssueEvent: public ReasonedEvent
{
    using base_type = ReasonedEvent;

public:
    explicit SaasIssueEvent(
        const QnResourcePtr& server,
        qint64 timeStamp,
        EventReason reasonCode,
        const QStringList& licenseKeys);

    explicit SaasIssueEvent(
        const QnResourcePtr& server,
        qint64 timeStamp,
        EventReason reasonCode,
        const UuidSet& resources);

    static bool isLicenseMigrationIssue(EventReason reasonCode);

    static QString encodeLicenseKeys(const QStringList& licenseKeys);
    static QString encodeResources(const UuidSet& resources);

    static QStringList decodeLicenseKeys(const EventParameters& params);
    static UuidSet decodeResources(const EventParameters& params);
};

} // namespace nx::vms::event
