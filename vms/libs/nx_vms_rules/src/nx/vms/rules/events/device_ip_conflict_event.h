// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API DeviceIpConflictEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.deviceIpConflict")

    FIELD(QnUuid, deviceId, setDeviceId)

public:
    DeviceIpConflictEvent(const QnUuid &deviceId):
        m_deviceId(deviceId)
    {
    }

    static FilterManifest filterManifest();
};

} // namespace nx::vms::rules
