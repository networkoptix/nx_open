#include "motion_event.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

namespace nx {
namespace vms {
namespace event {

MotionEvent::MotionEvent(
    const QnResourcePtr& resource,
    EventState toggleState,
    qint64 timeStamp,
    QnConstAbstractDataPacketPtr metadata)
    :
    base_type(cameraMotionEvent, resource, toggleState, timeStamp),
    m_metadata(metadata)
{
}

} // namespace event
} // namespace vms
} // namespace nx
