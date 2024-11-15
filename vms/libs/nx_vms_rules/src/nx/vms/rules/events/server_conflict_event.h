// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/camera_conflict_list.h>

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

/**
 * Server Conflict event can occur in two different scenarios.
 * First one - is when there is another server in our system, which has the same ID as ours. In
 * this case `conflicts` field has only `sourceServer` field filled with the conflicting server
 * address.
 * Second scenario is when the several servers in the local network are trying to control the same
 * set of cameras. In this case `sourceServer` field is our server, and `camerasByServer` are
 * filled by addressed of the servers and problematic cameras.
 */
class NX_VMS_RULES_API ServerConflictEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "serverConflict")

    FIELD(nx::Uuid, serverId, setServerId)
    FIELD(nx::vms::rules::CameraConflictList, conflicts, setConflicts)

public:
    ServerConflictEvent(
        std::chrono::microseconds timestamp,
        nx::Uuid serverId,
        const CameraConflictList& conflicts);

    ServerConflictEvent() = default;

    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo) const override;

    static const ItemDescriptor& manifest();

private:
    QStringList detailing() const;
    QString extendedCaption(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
