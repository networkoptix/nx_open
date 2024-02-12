// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API StorageIssueEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.storageIssue")

    FIELD(nx::Uuid, serverId, setServerId)
    FIELD(nx::vms::api::EventReason, reason, setReason)
    FIELD(QString, reasonText, setReasonText)

public:
    StorageIssueEvent() = default;
    StorageIssueEvent(
        std::chrono::microseconds timestamp,
        nx::Uuid serverId,
        nx::vms::api::EventReason reason,
        const QString& reasonText);

    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context) const override;
    virtual QString uniqueName() const override;

    static const ItemDescriptor& manifest();

private:
    QString extendedCaption(common::SystemContext* context) const;
    QString reason(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
