#ifndef __MOTION_BUSINESS_EVENT_H__
#define __MOTION_BUSINESS_EVENT_H__

#include <business/events/prolonged_business_event.h>

#include <core/datapacket/abstract_data_packet.h>
#include <core/resource/resource_fwd.h>

class QnMotionBusinessEvent: public QnProlongedBusinessEvent
{
    typedef QnProlongedBusinessEvent base_type;
public:
    QnMotionBusinessEvent(const QnResourcePtr& resource, QnBusiness::EventState toggleState, qint64 timeStamp, QnConstAbstractDataPacketPtr metadata);
private:
    QnConstAbstractDataPacketPtr m_metadata;
};

typedef QSharedPointer<QnMotionBusinessEvent> QnMotionBusinessEventPtr;

#endif // __MOTION_BUSINESS_EVENT_H__
