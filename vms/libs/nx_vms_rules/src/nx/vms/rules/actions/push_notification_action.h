// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/level.h>

#include "../basic_action.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API PushNotificationAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "pushNotification")

    FIELD(nx::vms::rules::UuidSelection, users, setUsers)
    FIELD(std::chrono::microseconds, interval, setInterval)
    FIELD(QString, caption, setCaption)
    FIELD(QString, description, setDescription)
    FIELD(bool, addSource, setAddSource)

    FIELD(nx::Uuid, serverId, setServerId)
    FIELD(UuidList, deviceIds, setDeviceIds)
    FIELD(nx::vms::event::Level, level, setLevel)

public:
    PushNotificationAction() = default;

    static const ItemDescriptor& manifest();
};

} // namespace nx::vms::rules
