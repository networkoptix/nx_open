// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>
#include <nx/vms/event/level.h>
#include <nx/vms/rules/client_action.h>
#include <nx/vms/rules/icon.h>

#include "../basic_action.h"
#include "../data_macros.h"
#include "../field_types.h"

namespace nx::vms::rules {

/** Base class for desktop client event tile based action. Should not be registered or created.*/
class NX_VMS_RULES_API NotificationActionBase: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.notificationBase")

    // Common fields.
    FIELD(nx::Uuid, id, setId)
    FIELD(QString, caption, setCaption)
    FIELD(QString, description, setDescription)
    FIELD(QString, tooltip, setTooltip)
    FIELD(nx::vms::rules::UuidSelection, users, setUsers)
    FIELD(nx::Uuid, serverId, setServerId)
    FIELD(UuidList, deviceIds, setDeviceIds)

    // Analytics data fields.
    FIELD(QString, objectTypeId, setObjectTypeId)
    FIELD(nx::Uuid, objectTrackId, setObjectTrackId)
    FIELD(nx::common::metadata::Attributes, attributes, setAttributes)

    // Notification look and feel fields.
    FIELD(QString, sourceName, setSourceName)
    FIELD(nx::vms::event::Level, level, setLevel)

protected:
    NotificationActionBase() = default;
};

} // namespace nx::vms::rules
