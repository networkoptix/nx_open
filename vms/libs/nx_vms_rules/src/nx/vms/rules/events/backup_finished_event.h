// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/event_rule_types.h>

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API BackupFinishedEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.archiveBackupFinished")

    FIELD(nx::Uuid, serverId, setServerId)
    FIELD(nx::vms::api::EventReason, reason, setReason)
    FIELD(QString, reasonText, setReasonText)

public:
    BackupFinishedEvent() = default;

    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo) const override;

    static const ItemDescriptor& manifest();

private:
    QString extendedCaption(common::SystemContext* context) const;
    QString reasonDetail() const;
};

} // namespace nx::vms::rules
