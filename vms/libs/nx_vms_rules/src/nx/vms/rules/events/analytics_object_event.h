// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsObjectEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.analyticsObject")

    FIELD(QnUuid, cameraId, setCameraId)
    FIELD(QnUuid, engineId, setEngineId)
    FIELD(QnUuid, objectTypeId, setObjectTypeId)
    FIELD(QnUuid, objectTrackId, setObjectTrackId)
    // TODO: Use of Attributes type leads to dependence cycle with common.
    // FIELD(nx::common::metadata::Attributes, attributes, setAttributes)

public:
    AnalyticsObjectEvent() = default;

    AnalyticsObjectEvent(
        QnUuid cameraId,
        QnUuid engineId,
        QnUuid objectTypeId,
        QnUuid objectTrackId,
        std::chrono::microseconds timestamp)
        :
        BasicEvent(timestamp),
        m_cameraId(cameraId),
        m_engineId(engineId),
        m_objectTypeId(objectTypeId),
        m_objectTrackId(objectTrackId)
    {
    }

    static FilterManifest filterManifest();
};

} // namespace nx::vms::rules
