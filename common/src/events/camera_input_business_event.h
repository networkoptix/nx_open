/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef CAMERA_INPUT_BUSINESS_EVENT_H
#define CAMERA_INPUT_BUSINESS_EVENT_H

#include "abstract_business_event.h"

namespace BusinessEventParameters
{
    static QLatin1String inputPortId( "inputPortId" );
}



class QnCameraInputEvent: public QnAbstractBusinessEvent
{
    typedef QnAbstractBusinessEvent base_type;
public:
    QnCameraInputEvent(
        QnResourcePtr resource,
        ToggleState::Value toggleState,
        qint64 timeStamp,
        const QString& inputPortID);

    virtual QString toString() const;

    const QString& inputPortID() const;

    virtual bool checkCondition(const QnBusinessParams &params) const override;

private:
    const QString m_inputPortID;
};

typedef QSharedPointer<QnCameraInputEvent> QnCameraInputEventPtr;

#endif //CAMERA_INPUT_BUSINESS_EVENT_H
