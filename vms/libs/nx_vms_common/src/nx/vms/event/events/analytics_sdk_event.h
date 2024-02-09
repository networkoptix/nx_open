// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/event/events/prolonged_event.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace event {

class NX_VMS_COMMON_API AnalyticsSdkEvent: public ProlongedEvent
{
    using base_type = ProlongedEvent;

public:
    AnalyticsSdkEvent(
        QnResourcePtr resource,
        QnUuid engineId,
        QString eventTypeId,
        EventState toggleState,
        QString caption,
        QString description,
        nx::common::metadata::Attributes attributes,
        QnUuid objectTrackId,
        QString key,
        qint64 timeStampUsec);

    QnUuid engineId() const;
    const QString& eventTypeId() const;

    QString caption() const { return m_caption; }
    QString description() const { return m_description; }

    virtual EventParameters getRuntimeParams() const override;
    virtual EventParameters getRuntimeParamsEx(
        const EventParameters& ruleEventParams) const override;

    virtual bool checkEventParams(const EventParameters &params) const override;

    virtual QString getExternalUniqueKey() const override;

    const nx::common::metadata::Attributes& attributes() const;
    const std::optional<QString> attribute(const QString& attributeName) const;

    const QnUuid objectTrackId() const;
    const QString& key() const;

private:
    const QnUuid m_engineId;
    const QString m_eventTypeId;
    const QString m_caption;
    const QString m_description;
    const nx::common::metadata::Attributes m_attributes;
    const QnUuid m_objectTrackId;
    const QString m_key;
};

} // namespace event
} // namespace vms
} // namespace nx
