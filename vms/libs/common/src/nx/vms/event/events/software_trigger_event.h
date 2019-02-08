#pragma once

#include <nx/vms/event/events/prolonged_event.h>

class QnUuid;

namespace nx {
namespace vms {
namespace event {

class SoftwareTriggerEvent: public ProlongedEvent
{
    using base_type = ProlongedEvent;

public:
    SoftwareTriggerEvent(const QnResourcePtr& resource,
        const QString& triggerId, const QnUuid& userId, qint64 timeStampUs,
        EventState toggleState = EventState::undefined);

    const QString& triggerId() const;

    const QnUuid& userId() const;

    virtual EventParameters getRuntimeParams() const override;
    virtual EventParameters getRuntimeParamsEx(
        const EventParameters& ruleEventParams) const override;

    virtual bool checkEventParams(const EventParameters &params) const override;

private:
    const QString m_triggerId;
    const QnUuid m_userId;
};

} // namespace event
} // namespace vms
} // namespace nx
