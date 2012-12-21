/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#include "camera_input_business_event.h"
#include "core/resource/resource_fwd.h"

namespace BusinessEventParameters {

    static QLatin1String inputPortId( "inputPortId" );

    QString getInputPortId(const QnBusinessParams &params) {
        return params.value(inputPortId, QString()).toString();
    }

    void setInputPortId(QnBusinessParams *params, const QString &value) {
        (*params)[inputPortId] = value;
    }

}


QnCameraInputEvent::QnCameraInputEvent(
    const QnResourcePtr& resource,
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
    text += QString::fromLatin1("  input port %1, state %2\n").arg(m_inputPortID)
            .arg(getToggleState() == ToggleState::On
                 ? QLatin1String("On")
                 : QLatin1String("Off"));
    return text;
}

const QString& QnCameraInputEvent::inputPortID() const
{
    return m_inputPortID;
}

bool QnCameraInputEvent::checkCondition(const QnBusinessParams &params) const {
    if (!base_type::checkCondition(params))
        return false;

    QString inputPort = BusinessEventParameters::getInputPortId(params);
    return inputPort.isEmpty() || inputPort == m_inputPortID;
}
