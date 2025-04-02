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

    virtual QString aggregationKey() const override { return m_serverId.toSimpleString(); }
    virtual QVariantMap details(
        common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo,
        Qn::ResourceInfoLevel detailLevel) const override;

    static const ItemDescriptor& manifest();

    static bool isLicenseMigrationIssue(nx::vms::api::EventReason reason);

protected:
    virtual QString extendedCaption(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

private:
    QString reason(nx::vms::common::SystemContext* context) const;
    QString serviceDisabledReason(common::SystemContext* context) const;
    QString licenseMigrationReason() const;
    bool isLicenseMigrationIssue() const;

    std::pair<QString, QStringList> detailing(nx::vms::common::SystemContext* context) const;
};

} // namespace nx::vms::rules
