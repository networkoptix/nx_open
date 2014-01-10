/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#include "camera_input_business_event.h"

QnCameraInputEvent::QnCameraInputEvent(const QnResourcePtr& resource, Qn::ToggleState toggleState, qint64 timeStamp, const QString& inputPortID):
    base_type(BusinessEventType::Camera_Input, resource, toggleState, timeStamp),
    m_inputPortID(inputPortID)
{
}

const QString& QnCameraInputEvent::inputPortID() const
{
    return m_inputPortID;
}

bool QnCameraInputEvent::checkCondition(Qn::ToggleState state, const QnBusinessEventParameters &params) const {
    if (!base_type::checkCondition(state, params))
        return false;

    QString inputPort = params.getInputPortId();
    return inputPort.isEmpty() || inputPort == m_inputPortID;
}

QnBusinessEventParameters QnCameraInputEvent::getRuntimeParams() const {
    QnBusinessEventParameters params = base_type::getRuntimeParams();
    params.setInputPortId(m_inputPortID);
    return params;
}
