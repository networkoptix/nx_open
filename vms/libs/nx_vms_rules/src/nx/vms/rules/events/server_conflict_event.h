// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/camera_conflict_list.h>

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ServerConflictEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.serverConflict")

    FIELD(nx::Uuid, serverId, setServerId)
    FIELD(nx::vms::rules::CameraConflictList, conflicts, setConflicts)

public:
    ServerConflictEvent(
        std::chrono::microseconds timestamp,
        nx::Uuid serverId,
        const CameraConflictList& conflicts);

    ServerConflictEvent() = default;

    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context) const override;

    static const ItemDescriptor& manifest();

private:
    QStringList detailing() const;
    QString extendedCaption(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
