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
        const QnResourcePtr& resource,
        const QString& pluginId,
        const QString& eventTypeId,
        EventState toggleState,
        const QString& caption,
        const QString& description,
        const QString& auxiliaryData,
        qint64 timeStampUsec);

    QString pluginId() const;
    const QString& eventTypeId() const;

    virtual EventParameters getRuntimeParams() const override;
    virtual EventParameters getRuntimeParamsEx(
        const EventParameters& ruleEventParams) const override;

    virtual bool checkEventParams(const EventParameters &params) const override;

    QString auxiliaryData() const;

private:
    const QString m_pluginId;
    const QString m_eventTypeId;
    const QString m_caption;
    const QString m_description;
    const QString m_auxiliaryData;
};

} // namespace event
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::event::AnalyticsSdkEventPtr);
