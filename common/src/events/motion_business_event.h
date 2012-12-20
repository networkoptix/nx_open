#ifndef __MOTION_BUSINESS_EVENT_H__
#define __MOTION_BUSINESS_EVENT_H__

#include "abstract_business_event.h"
#include "core/datapacket/media_data_packet.h"

class QnMotionBusinessEvent: public QnAbstractBusinessEvent
{
public:
    QnMotionBusinessEvent(QnResourcePtr resource,
                          ToggleState::Value toggleState,
                          qint64 timeStamp,
                          QnMetaDataV1Ptr metadata);
private:
    QnMetaDataV1Ptr m_metadata;
};

typedef QSharedPointer<QnMotionBusinessEvent> QnMotionBusinessEventPtr;

#endif // __MOTION_BUSINESS_EVENT_H__
