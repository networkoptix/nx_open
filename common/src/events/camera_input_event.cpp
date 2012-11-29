/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#include "camera_input_event.h"


QnCameraInputEvent::QnCameraInputEvent(
    QnResourcePtr resource,
    const QString& inputPortID,
    ToggleState toggleState,
    qint64 timestamp )
:
    m_inputPortID( inputPortID )
{
    setResource( resource );
    setToggleState( toggleState );
    setDateTime( timestamp );
}

bool QnCameraInputEvent::checkCondition( const QnBusinessParams& /*params*/ ) const
{
    return true;
}

const QString& QnCameraInputEvent::inputPortID() const
{
    return m_inputPortID;
}
