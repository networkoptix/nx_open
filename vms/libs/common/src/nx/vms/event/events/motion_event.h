#pragma once

#include <nx/vms/event/events/prolonged_event.h>

#include <nx/streaming/abstract_data_packet.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace vms {
namespace event {

class MotionEvent: public ProlongedEvent
{
    using base_type = ProlongedEvent;

public:
    MotionEvent(const QnResourcePtr& resource, EventState toggleState,
        qint64 timeStamp, QnConstAbstractDataPacketPtr metadata);

private:
    QnConstAbstractDataPacketPtr m_metadata;
};

} // namespace event
} // namespace vms
} // namespace nx
