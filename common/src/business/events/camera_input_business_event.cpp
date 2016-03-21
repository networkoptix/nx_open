/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#include "camera_input_business_event.h"

QnCameraInputEvent::QnCameraInputEvent(const QnResourcePtr& resource, QnBusiness::EventState toggleState, qint64 timeStamp, const QString& inputPortID):
    base_type(QnBusiness::CameraInputEvent, resource, toggleState, timeStamp),
    m_inputPortID(inputPortID)
{
}

const QString& QnCameraInputEvent::inputPortID() const
{
    return m_inputPortID;
}

bool QnCameraInputEvent::checkEventParams(const QnBusinessEventParameters &params) const 
{
    QString inputPort = params.inputPortId;
    return inputPort.isEmpty() || inputPort == m_inputPortID;
}

QnBusinessEventParameters QnCameraInputEvent::getRuntimeParams() const {
    QnBusinessEventParameters params = base_type::getRuntimeParams();
    params.inputPortId = m_inputPortID;
    return params;
}
