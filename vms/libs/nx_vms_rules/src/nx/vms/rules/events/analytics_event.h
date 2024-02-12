// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>

#include "../data_macros.h"
#include "analytics_engine_event.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsEvent: public AnalyticsEngineEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.analytics")

    FIELD(QString, eventTypeId, setEventTypeId)
    FIELD(nx::common::metadata::Attributes, attributes, setAttributes)
    FIELD(nx::Uuid, objectTrackId, setObjectTrackId)
    FIELD(QString, key, setKey)

public:
    AnalyticsEvent() = default;

    AnalyticsEvent(
        std::chrono::microseconds timestamp,
        State state,
        const QString &caption,
        const QString &description,
        nx::Uuid cameraId,
        nx::Uuid engineId,
        const QString& eventTypeId,
        const nx::common::metadata::Attributes& attributes,
        nx::Uuid objectTrackId,
        const QString& key);

    virtual QString subtype() const override;
    virtual QString resourceKey() const override;
    virtual QString aggregationKey() const override;
    virtual QVariantMap details(common::SystemContext* context) const override;

    static const ItemDescriptor& manifest();

private:
    QString analyticsEventCaption(common::SystemContext* context) const;
    QString extendedCaption(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
