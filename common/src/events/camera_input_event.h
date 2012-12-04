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
        const QString& inputPortID,
        ToggleState::Value toggleState,
        qint64 timestamp );

    virtual QString toString() const;

    const QString& inputPortID() const;

private:
    const QString m_inputPortID;
};

typedef QSharedPointer<QnCameraInputEvent> QnCameraInputEventPtr;

#endif //CAMERA_INPUT_EVENT_H
