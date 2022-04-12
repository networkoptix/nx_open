// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/instant_event.h>
#include <nx/vms/event/events/events_fwd.h>

class QnUuid;

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API AnalyticsSdkObjectDetected: public InstantEvent
{
    using base_type = InstantEvent;

public:
    AnalyticsSdkObjectDetected(
        QnResourcePtr resource,
        QnUuid engineId,
        QString objectTypeId,
        nx::common::metadata::Attributes attributes,
        QnUuid objectTrackId,
        qint64 timeStampUsec);

    QnUuid engineId() const;
    const QString& objectTypeId() const;
    QnUuid objectTrackId() const;

    virtual EventParameters getRuntimeParams() const override;
    virtual EventParameters getRuntimeParamsEx(
        const EventParameters& ruleEventParams) const override;

    virtual bool checkEventParams(const EventParameters &params) const override;

    virtual QString getExternalUniqueKey() const override;

    const nx::common::metadata::Attributes& attributes() const;
    const std::optional<QString> attribute(const QString& attributeName) const;

private:
    const QnUuid m_engineId;
    const QString m_objectTypeId;
    const nx::common::metadata::Attributes m_attributes;
    const QnUuid m_objectTrackId;
};

} // namespace event
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::event::AnalyticsSdkObjectDetectedPtr);
