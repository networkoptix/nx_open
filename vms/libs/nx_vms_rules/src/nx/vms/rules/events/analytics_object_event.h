// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsObjectEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.analyticsObject")

    FIELD(QnUuid, cameraId, setCameraId)
    FIELD(QnUuid, engineId, setEngineId)
    FIELD(QString, objectTypeId, setObjectTypeId)
    FIELD(QnUuid, objectTrackId, setObjectTrackId)
    FIELD(nx::common::metadata::Attributes, attributes, setAttributes)

public:
    AnalyticsObjectEvent() = default;

    AnalyticsObjectEvent(
        std::chrono::microseconds timestamp,
        QnUuid cameraId,
        QnUuid engineId,
        const QString& objectTypeId,
        QnUuid objectTrackId,
        const nx::common::metadata::Attributes& attributes);

    static const ItemDescriptor& manifest();
};

} // namespace nx::vms::rules
