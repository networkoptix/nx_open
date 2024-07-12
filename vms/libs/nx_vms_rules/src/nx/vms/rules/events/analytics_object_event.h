// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>

#include "../data_macros.h"
#include "analytics_engine_event.h"

namespace nx::analytics::taxonomy { class AbstractObjectType; }

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsObjectEvent: public AnalyticsEngineEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.events.analyticsObject")

    FIELD(QString, objectTypeId, setObjectTypeId)
    FIELD(nx::Uuid, objectTrackId, setObjectTrackId)
    FIELD(nx::common::metadata::Attributes, attributes, setAttributes)

public:
    AnalyticsObjectEvent() = default;

    AnalyticsObjectEvent(
        std::chrono::microseconds timestamp,
        nx::Uuid cameraId,
        nx::Uuid engineId,
        const QString& objectTypeId,
        nx::Uuid objectTrackId,
        const nx::common::metadata::Attributes& attributes);

    virtual QString subtype() const override;
    virtual QString resourceKey() const override;
    virtual QString aggregationKey() const override;
    virtual QString cacheKey() const override;
    virtual QVariantMap details(common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo) const override;

    static const ItemDescriptor& manifest();

private:
    QString analyticsObjectCaption(common::SystemContext* context) const;
    QString extendedCaption(common::SystemContext* context) const;
    nx::analytics::taxonomy::AbstractObjectType* objectTypeById(common::SystemContext* context) const;
};

} // namespace nx::vms::rules
