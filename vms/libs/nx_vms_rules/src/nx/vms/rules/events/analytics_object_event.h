// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>

#include "../data_macros.h"
#include "analytics_engine_event.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsObjectEvent: public AnalyticsEngineEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.analyticsObject")

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

    virtual QString uniqueName() const override;
    virtual QString resourceKey() const override;
    virtual QVariantMap details(common::SystemContext* context) const override;

    static const ItemDescriptor& manifest();

private:
    QString analyticsObjectCaption(common::SystemContext* context) const;
    QString extendedCaption(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
