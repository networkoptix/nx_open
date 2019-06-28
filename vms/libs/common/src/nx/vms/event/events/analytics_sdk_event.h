#pragma once

#include <nx/vms/event/events/prolonged_event.h>
#include <nx/vms/event/events/events_fwd.h>

class QnUuid;

namespace nx {
namespace vms {
namespace event {

class AnalyticsSdkEvent: public ProlongedEvent
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
        std::map<QString, QString> attributes,
        qint64 timeStampUsec);

    QnUuid engineId() const;
    const QString& eventTypeId() const;

    virtual EventParameters getRuntimeParams() const override;
    virtual EventParameters getRuntimeParamsEx(
        const EventParameters& ruleEventParams) const override;

    virtual bool checkEventParams(const EventParameters &params) const override;

    const std::map<QString, QString>& attributes() const;
    const std::optional<QString> attribute(const QString& attributeName) const;

private:
    const QnUuid m_engineId;
    const QString m_eventTypeId;
    const QString m_caption;
    const QString m_description;
    const std::map<QString, QString> m_attributes;
};

} // namespace event
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::event::AnalyticsSdkEventPtr);
