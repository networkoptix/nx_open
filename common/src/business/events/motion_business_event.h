#ifndef __MOTION_BUSINESS_EVENT_H__
#define __MOTION_BUSINESS_EVENT_H__

#include "prolonged_business_event.h"

#include <core/datapacket/abstract_data_packet.h>
#include <core/resource/resource_fwd.h>

class QnMotionBusinessEvent: public QnProlongedBusinessEvent
{
    typedef QnProlongedBusinessEvent base_type;
public:
    QnMotionBusinessEvent(const QnResourcePtr& resource, Qn::ToggleState toggleState, qint64 timeStamp, QnAbstractDataPacketPtr metadata);

    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static bool isResourcesListValid(const QnResourceList &resources);
    static int  invalidResourcesCount(const QnResourceList &resources);
private:
    QnAbstractDataPacketPtr m_metadata;
};

typedef QSharedPointer<QnMotionBusinessEvent> QnMotionBusinessEventPtr;

#endif // __MOTION_BUSINESS_EVENT_H__
