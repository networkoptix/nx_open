// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API LdapSyncIssueEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "ldapSyncIssue")
    FIELD(nx::Uuid, serverId, setServerId)
    FIELD(nx::vms::api::EventReason, reason, setReason)
    FIELD(std::chrono::seconds, syncInterval, setSyncInterval)

public:
    LdapSyncIssueEvent() = default;
    LdapSyncIssueEvent(
        std::chrono::microseconds timestamp,
        nx::vms::api::EventReason reason,
        std::chrono::seconds syncInterval,
        nx::Uuid serverId);

    virtual QString aggregationKey() const override { return m_serverId.toSimpleString(); }
    virtual QVariantMap details(
        common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo,
        Qn::ResourceInfoLevel detailLevel) const override;

    virtual nx::vms::api::rules::PropertyMap aggregatedInfo(
        const AggregatedEvent& aggregatedEvent) const override;

    static const ItemDescriptor& manifest();

protected:
    virtual QString extendedCaption(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

private:
    QString reasonText(vms::api::EventReason reason) const;
    QString reasonText(const nx::vms::api::rules::PropertyMap& aggregatedInfo = {}) const;
};

} // namespace nx::vms::rules
