// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.analytics")

    FIELD(QnUuid, cameraId, setCameraId)
    // TODO: Introduce new type of 2 ids, engineId.typeId?
    FIELD(QnUuid, engineId, setEngineId)
    FIELD(QnUuid, eventTypeId, setEventTypeId)
    FIELD(QString, caption, setCaption)
    FIELD(QString, description, setDescription)

public:
    AnalyticsEvent() = default;

    AnalyticsEvent(
        QnUuid cameraId,
        QnUuid engineId,
        QnUuid eventTypeId,
        const QString &caption,
        const QString &description,
        std::chrono::microseconds timestamp)
        :
        BasicEvent(timestamp),
        m_cameraId(cameraId),
        m_engineId(engineId),
        m_eventTypeId(eventTypeId),
        m_caption(caption),
        m_description(description)
    {
    }

    static FilterManifest filterManifest();
};

} // namespace nx::vms::rules
