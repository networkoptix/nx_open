// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API SoftTriggerEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.softTrigger")

    FIELD(QnUuid, cameraId, setCameraId)
    FIELD(QnUuid, userId, setUserId)

public:
    SoftTriggerEvent() = default;
    SoftTriggerEvent(const QnUuid &cameraId, const QnUuid &userId):
        m_cameraId(cameraId),
        m_userId(userId)
    {
    }

    static FilterManifest filterManifest();
    static const ItemDescriptor& manifest();
};

} // namespace nx::vms::rules
