// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API SaasIssueEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "saasIssue")

    FIELD(nx::Uuid, serverId, setServerId)
    FIELD(QStringList, licenseKeys, setLicenseKeys)
    FIELD(UuidList, deviceIds, setDeviceIds)
    FIELD(nx::vms::api::EventReason, reason, setReason)

public:
    SaasIssueEvent() = default;
    SaasIssueEvent(
        std::chrono::microseconds timestamp,
        nx::Uuid serverId,
        const QStringList& licenseKeys,
        nx::vms::api::EventReason reason);
    SaasIssueEvent(
        std::chrono::microseconds timestamp,
        nx::Uuid serverId,
        const UuidList& deviceIds,
        nx::vms::api::EventReason reason);

    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo) const override;

    static const ItemDescriptor& manifest();

private:
    QString extendedCaption() const;
    QString reason(nx::vms::common::SystemContext* context) const;
    QString serviceDisabledReason(common::SystemContext* context) const;
    QString licenseMigrationReason() const;
    QStringList detailing(common::SystemContext* context) const;
    bool isLicenseMigrationIssue() const;
};

} // namespace nx::vms::rules
