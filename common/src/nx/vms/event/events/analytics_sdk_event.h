#pragma once

#include <nx/vms/event/events/prolonged_event.h>

class QnUuid;

namespace nx {
namespace vms {
namespace event {

class AnalyticsSdkEvent: public ProlongedEvent
{
    using base_type = ProlongedEvent;

public:
    AnalyticsSdkEvent(const QnResourcePtr& resource,
        const QnUuid& driverId,
        const QnUuid& eventId,
        EventState toggleState,
        const QString& caption,
        const QString& description,
        qint64 timeStampUsec);

    const QnUuid& driverId() const;
    const QnUuid& eventId() const;

    virtual EventParameters getRuntimeParams() const override;
    virtual EventParameters getRuntimeParamsEx(
        const EventParameters& ruleEventParams) const override;

    virtual bool checkEventParams(const EventParameters &params) const override;

private:
    const QnUuid m_driverId;
    const QnUuid m_eventId;
    const QString m_caption;
    const QString m_description;
};

} // namespace event
} // namespace vms
} // namespace nx
