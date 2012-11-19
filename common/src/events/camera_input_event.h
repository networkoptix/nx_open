/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef CAMERA_INPUT_EVENT_H
#define CAMERA_INPUT_EVENT_H

#include "abstract_business_event.h"


class QnCameraInputEvent
:
    public QnAbstractBusinessEvent
{
public:
    QnCameraInputEvent(
        QnResourcePtr resource,
        const QString& inputToken,
        ToggleState toggleState,
        qint64 timestamp );

    //!Implementation of QnAbstractBusinessEvent::checkCondition
    /*!
        Always returns true
    */
    virtual bool checkCondition( const QnBusinessParams& params ) const override;

    const QString& inputToken() const;

private:
    const QString m_inputToken;
};

typedef QSharedPointer<QnCameraInputEvent> QnCameraInputEventPtr;

#endif //CAMERA_INPUT_EVENT_H
