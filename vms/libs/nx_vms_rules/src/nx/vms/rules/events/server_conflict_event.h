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

    FIELD(QnUuid, source, setSource)
    FIELD(nx::vms::rules::CameraConflictList, conflicts, setConflicts)

public:
    static const ItemDescriptor& manifest();

    ServerConflictEvent(
        std::chrono::microseconds timestamp,
        QnUuid serverId,
        const CameraConflictList& conflicts);

    ServerConflictEvent() = default;

    virtual QMap<QString, QString> details(common::SystemContext* context) const override;

private:
    QString detailing() const;
};

} // namespace nx::vms::rules
