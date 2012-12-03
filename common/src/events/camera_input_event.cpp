/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#include "camera_input_event.h"


QnCameraInputEvent::QnCameraInputEvent(
    QnResourcePtr resource,
    const QString& inputPortID,
    ToggleState::Value toggleState,
    qint64 timestamp )
:
    m_inputPortID( inputPortID )
{
    setEventType( BusinessEventType::BE_Camera_Input );
    setResource( resource );
    setToggleState( toggleState );
    setDateTime( timestamp );
}

QString QnCameraInputEvent::toString() const
{
    QString text = QnAbstractBusinessEvent::toString();
    text += QString::fromLatin1("  input port %1, state %2\n").arg(m_inputPortID).arg(ToggleState::toString(getToggleState()));
    return text;
}

bool QnCameraInputEvent::checkCondition( const QnBusinessParams& /*params*/ ) const
{
    return true;
}

const QString& QnCameraInputEvent::inputPortID() const
{
    return m_inputPortID;
}
