/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#include "camera_input_business_event.h"
#include "core/resource/resource.h"

QnCameraInputEvent::QnCameraInputEvent(
    QnResourcePtr resource,
    ToggleState::Value toggleState,
    qint64 timeStamp,
    const QString& inputPortID)
:
    base_type(
        BusinessEventType::BE_Camera_Input,
        resource,
        toggleState,
        timeStamp),
    m_inputPortID( inputPortID )
{
}

QString QnCameraInputEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    text += QString::fromLatin1("  input port %1, state %2\n").arg(m_inputPortID).arg(QLatin1String(ToggleState::toString(getToggleState())));
    return text;
}

const QString& QnCameraInputEvent::inputPortID() const
{
    return m_inputPortID;
}

bool QnCameraInputEvent::checkCondition(const QnBusinessParams &params) const {
    bool result = base_type::checkCondition(params);
    if (!result)
        return false;

    QVariant inputPort = getParameter(params, BusinessEventParameters::inputPortId);
    if (!inputPort.isValid())
        return true; // no condition on input port => work on any

    QString requiredPort = inputPort.toString();
    return requiredPort.isEmpty() || requiredPort == m_inputPortID;
}
