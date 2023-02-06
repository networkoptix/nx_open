// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "notification_action_base.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ShowOnAlarmLayoutAction: public NotificationActionBase
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.showOnAlarmLayout")

    FIELD(QnUuidList, eventDeviceIds, setEventDeviceIds)
    FIELD(std::chrono::microseconds, interval, setInterval)
    FIELD(std::chrono::microseconds, playbackTime, setPlaybackTime)
    FIELD(bool, forceOpen, setForceOpen)

public:
    ShowOnAlarmLayoutAction() = default;
    static const ItemDescriptor& manifest();
};

} // namespace nx::vms::rules
