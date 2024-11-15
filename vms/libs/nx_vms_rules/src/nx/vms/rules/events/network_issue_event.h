// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/rules/network_issue_info.h>

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

struct NetworkIssueInfo;

class NX_VMS_RULES_API NetworkIssueEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "networkIssue")

    FIELD(nx::Uuid, cameraId, setCameraId)
    FIELD(nx::vms::api::EventReason, reason, setReason)
    FIELD(nx::vms::rules::NetworkIssueInfo, info, setInfo)

public:
    NetworkIssueEvent() = default;
    NetworkIssueEvent(
        std::chrono::microseconds timestamp,
        nx::Uuid cameraId,
        nx::vms::api::EventReason reason,
        const NetworkIssueInfo& info);

    virtual QString resourceKey() const override;
    virtual QString aggregationSubKey() const override;
    virtual QVariantMap details(common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo) const override;

    static const ItemDescriptor& manifest();

private:
    QString extendedCaption(common::SystemContext* context) const;
    QString reasonText(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
