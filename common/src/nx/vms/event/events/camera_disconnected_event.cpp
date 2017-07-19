#include "camera_disconnected_event.h"
#include "core/resource/resource.h"

namespace nx {
namespace vms {
namespace event {

CameraDisconnectedEvent::CameraDisconnectedEvent(
    const QnResourcePtr& cameraResource,
    qint64 timeStamp)
    :
    base_type(cameraDisconnectEvent, cameraResource, timeStamp)
{
}

} // namespace event
} // namespace vms
} // namespace nx
