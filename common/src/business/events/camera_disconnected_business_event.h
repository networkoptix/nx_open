#ifndef __CAMERA_DISCONNECTED_BUSINESS_EVENT_H__
#define __CAMERA_DISCONNECTED_BUSINESS_EVENT_H__

#include "instant_business_event.h"
#include "core/resource/resource_fwd.h"

class QnCameraDisconnectedBusinessEvent: public QnInstantBusinessEvent
{
    typedef QnInstantBusinessEvent base_type;
public:
    QnCameraDisconnectedBusinessEvent(const QnResourcePtr& cameraResource, qint64 timeStamp);

    /**
     * @brief checkCondition
     * @params params 'camera' - optional camera unique ID
     */
    bool checkCondition(QnBusiness::EventState state, const QnBusinessEventParameters& params) const override;
};

typedef QSharedPointer<QnCameraDisconnectedBusinessEvent> QnCameraDisconnectedBusinessEventPtr;

#endif // __CAMERA_DISCONNECTED_BUSINESS_EVENT_H__
