// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_event.h>

#include "../data_macros.h"
#include "analytics_engine_event.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsEvent: public AnalyticsEngineEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.analytics")

    // TODO: Introduce new type of 2 ids, engineId.typeId?
    FIELD(QnUuid, eventTypeId, setEventTypeId)

public:
    AnalyticsEvent() = default;

    AnalyticsEvent(
        std::chrono::microseconds timestamp,
        const QString &caption,
        const QString &description,
        QnUuid cameraId,
        QnUuid engineId,
        QnUuid eventTypeId)
        :
        AnalyticsEngineEvent(timestamp, caption, description, cameraId, engineId),
        m_eventTypeId(eventTypeId)
    {
    }

    virtual QMap<QString, QString> details(common::SystemContext* context) const override;

    static FilterManifest filterManifest();

private:
    QString analyticsEventCaption(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
