#ifndef __MOTION_BUSINESS_EVENT_H__
#define __MOTION_BUSINESS_EVENT_H__

#include "prolonged_business_event.h"
#include "core/datapacket/abstract_data_packet.h"

class QnMotionBusinessEvent: public QnProlongedBusinessEvent
{
    typedef QnProlongedBusinessEvent base_type;
public:
    QnMotionBusinessEvent(const QnResourcePtr& resource,
                          ToggleState::Value toggleState,
                          qint64 timeStamp,
                          QnAbstractDataPacketPtr metadata);
private:
    QnAbstractDataPacketPtr m_metadata;
};

typedef QSharedPointer<QnMotionBusinessEvent> QnMotionBusinessEventPtr;

#endif // __MOTION_BUSINESS_EVENT_H__
