#pragma once

#include <QtCore/QString>

#include <nx/vms/event/events/instant_event.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace event {

class ConflictEvent: public InstantEvent
{
    using base_type = InstantEvent;

public:
    explicit ConflictEvent(
        const EventType eventType,
        const QnResourcePtr& resource,
        const qint64 timeStamp,
        const QString& caption = QString(),
        const QString& description = QString());

    virtual EventParameters getRuntimeParams() const override;

protected:
    QString m_caption;
    QString m_description;
};

} // namespace event
} // namespace vms
} // namespace nx
