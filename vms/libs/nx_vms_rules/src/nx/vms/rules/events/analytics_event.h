// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>

#include "../basic_event.h"
#include "../data_macros.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API AnalyticsEvent: public BasicEvent
{
    Q_OBJECT
    Q_CLASSINFO("type", "analytics")
    FIELD(nx::Uuid, deviceId, setDeviceId)
    FIELD(nx::Uuid, engineId, setEngineId)
    FIELD(QString, caption, setCaption)
    FIELD(QString, description, setDescription)
    FIELD(QString, eventTypeId, setEventTypeId)
    FIELD(nx::common::metadata::Attributes, attributes, setAttributes)
    FIELD(nx::Uuid, objectTrackId, setObjectTrackId)
    FIELD(QString, key, setKey)
    FIELD(QRectF, boundingBox, setBoundingBox)

public:
    AnalyticsEvent() = default;
    AnalyticsEvent(
        std::chrono::microseconds timestamp,
        State state,
        const QString& caption,
        const QString& description,
        nx::Uuid deviceId,
        nx::Uuid engineId,
        const QString& eventTypeId,
        const nx::common::metadata::Attributes& attributes,
        nx::Uuid objectTrackId,
        const QString& key,
        const QRectF& boundingBox);

    virtual QString aggregationKey() const override { return m_deviceId.toSimpleString(); }
    virtual QString subtype() const override;
    virtual QString sequenceKey() const override;
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
    QString analyticsEventCaption(common::SystemContext* context) const;
    QStringList detailing() const;
};

} // namespace nx::vms::rules
