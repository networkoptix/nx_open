// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/rules/network_issue_info.h>

namespace nx::vms::rules {

struct NetworkIssueInfo;

class NX_VMS_RULES_API NetworkIssueEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.networkIssue")

    FIELD(QnUuid, cameraId, setCameraId)
    FIELD(nx::vms::api::EventReason, reason, setReason)
    FIELD(nx::vms::rules::NetworkIssueInfo, info, setInfo)

public:
    NetworkIssueEvent() = default;
    NetworkIssueEvent(
        std::chrono::microseconds timestamp,
        QnUuid cameraId,
        nx::vms::api::EventReason reason,
        const NetworkIssueInfo& info);

    virtual QString uniqueName() const override;
    virtual QVariantMap details(common::SystemContext* context) const override;

    static const ItemDescriptor& manifest();

private:
    QString extendedCaption(common::SystemContext* context) const;
    QString reason(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
