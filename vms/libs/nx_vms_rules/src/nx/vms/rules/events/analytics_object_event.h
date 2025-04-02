// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::analytics::taxonomy { class AbstractObjectType; }

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsObjectEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "analyticsObject")
    FIELD(nx::Uuid, deviceId, setDeviceId)
    FIELD(nx::Uuid, engineId, setEngineId)
    FIELD(QString, objectTypeId, setObjectTypeId)
    FIELD(nx::Uuid, objectTrackId, setObjectTrackId)
    FIELD(nx::common::metadata::Attributes, attributes, setAttributes)

public:
    AnalyticsObjectEvent() = default;
    AnalyticsObjectEvent(
        State state,
        std::chrono::microseconds timestamp,
        nx::Uuid deviceId,
        nx::Uuid engineId,
        const QString& objectTypeId,
        nx::Uuid objectTrackId,
        const nx::common::metadata::Attributes& attributes);

    virtual QString aggregationKey() const override { return m_deviceId.toSimpleString(); }
    virtual QString subtype() const override;
    virtual QString sequenceKey() const override;
    virtual QString cacheKey() const override;
    virtual QVariantMap details(
        common::SystemContext* context,
        const nx::vms::api::rules::PropertyMap& aggregatedInfo,
        Qn::ResourceInfoLevel detailLevel) const override;

    static const ItemDescriptor& manifest();

protected:
    virtual QString extendedCaption(
        common::SystemContext* context,
        Qn::ResourceInfoLevel detailLevel) const override;

private:
    QString analyticsObjectCaption(common::SystemContext* context) const;
    nx::analytics::taxonomy::AbstractObjectType* objectTypeById(
        common::SystemContext* context) const;
};

} // namespace nx::vms::rules
