#pragma once
/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#include <nx/vms/event/events/prolonged_event.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace event {

class CameraInputEvent: public ProlongedEvent
{
    using base_type = ProlongedEvent;

public:
    CameraInputEvent(const QnResourcePtr& resource, EventState toggleState,
        qint64 timeStamp, const QString& inputPortID);

    const QString& inputPortID() const;

    virtual bool checkEventParams(const EventParameters &params) const override;
    virtual EventParameters getRuntimeParams() const override;

private:
    const QString m_inputPortID;
};

} // namespace event
} // namespace vms
} // namespace nx
