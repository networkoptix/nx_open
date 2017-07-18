/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#include "camera_input_event.h"

namespace nx {
namespace vms {
namespace event {

CameraInputEvent::CameraInputEvent(
    const QnResourcePtr& resource,
    EventState toggleState,
    qint64 timeStamp,
    const QString& inputPortID)
    :
    base_type(cameraInputEvent, resource, toggleState, timeStamp),
    m_inputPortID(inputPortID)
{
}

const QString& CameraInputEvent::inputPortID() const
{
    return m_inputPortID;
}

bool CameraInputEvent::checkEventParams(const EventParameters& params) const
{
    QString inputPort = params.inputPortId;
    return inputPort.isEmpty() || inputPort == m_inputPortID;
}

EventParameters CameraInputEvent::getRuntimeParams() const
{
    EventParameters params = base_type::getRuntimeParams();
    params.inputPortId = m_inputPortID;
    return params;
}

} // namespace event
} // namespace vms
} // namespace nx
