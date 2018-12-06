#pragma once

#include <nx/vms/event/events/abstract_event.h>
#include <core/resource/resource_fwd.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace event {

class CustomEvent: public AbstractEvent
{
    using base_type = AbstractEvent;

public:
    explicit CustomEvent(
        EventState toggleState,
        qint64 timeStamp, const
        QString& resourceName,
        const QString& caption,
        const QString& description,
        EventMetaData metadata);

    virtual bool isEventStateMatched(EventState state, ActionType actionType) const override;
    virtual bool checkEventParams(const EventParameters& params) const override;
    virtual EventParameters getRuntimeParams() const override;

private:
    const QString m_resourceName;
    const QString m_caption;
    const QString m_description;
    const EventMetaData m_metadata;
};

} // namespace event
} // namespace vms
} // namespace nx
