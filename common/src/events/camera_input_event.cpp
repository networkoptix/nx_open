/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#include "camera_input_event.h"


QnCameraInputEvent::QnCameraInputEvent(
    QnResourcePtr resource,
    const QString& inputToken,
    ToggleState toggleState,
    qint64 timestamp )
:
    m_inputToken( inputToken )
{
    setResource( resource );
    setToggleState( toggleState );
    setDateTime( timestamp );
}

bool QnCameraInputEvent::checkCondition( const QnBusinessParams& /*params*/ ) const
{
    return true;
}

const QString& QnCameraInputEvent::inputToken() const
{
    return m_inputToken;
}
