/**********************************************************
* 13 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef CAMERA_INPUT_BUSINESS_EVENT_H
#define CAMERA_INPUT_BUSINESS_EVENT_H

#include <business/events/prolonged_business_event.h>
#include <core/resource/resource_fwd.h>

class QnCameraInputEvent: public QnProlongedBusinessEvent {
    typedef QnProlongedBusinessEvent base_type;
public:
    QnCameraInputEvent(const QnResourcePtr& resource, QnBusiness::EventState toggleState, qint64 timeStamp, const QString& inputPortID);

    const QString& inputPortID() const;

    virtual bool checkCondition(QnBusiness::EventState state, const QnBusinessEventParameters &params) const override;

    virtual QnBusinessEventParameters getRuntimeParams() const override;
private:
    const QString m_inputPortID;
};

typedef QSharedPointer<QnCameraInputEvent> QnCameraInputEventPtr;

#endif //CAMERA_INPUT_BUSINESS_EVENT_H
