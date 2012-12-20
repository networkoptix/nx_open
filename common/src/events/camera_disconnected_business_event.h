#ifndef __CAMERA_DISCONNECTED_BUSINESS_EVENT_H__
#define __CAMERA_DISCONNECTED_BUSINESS_EVENT_H__

#include "abstract_business_event.h"
#include "core/datapacket/media_data_packet.h"

class QnCameraDisconnectedBusinessEvent: public QnAbstractBusinessEvent
{
public:
    QnCameraDisconnectedBusinessEvent(QnResourcePtr mediaServerResource, QnResourcePtr cameraResource, qint64 timeStamp);

    /**
     * @brief checkCondition
     * @params params 'camera' - optional camera unique ID
     */
    bool checkCondition(const QnBusinessParams& params) const;

    virtual QString toString() const override;
private:
    QnResourcePtr m_cameraResource;
};

typedef QSharedPointer<QnCameraDisconnectedBusinessEvent> QnCameraDisconnectedBusinessEventPtr;

#endif // __CAMERA_DISCONNECTED_BUSINESS_EVENT_H__
