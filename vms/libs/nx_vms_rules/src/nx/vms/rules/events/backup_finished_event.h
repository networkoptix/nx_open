// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API BackupFinishedEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.archiveBackupFinished")

    FIELD(nx::Uuid, serverId, setServerId)

public:
    BackupFinishedEvent() = default;

    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context) const override;

    static const ItemDescriptor& manifest();

private:
    QString extendedCaption(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
