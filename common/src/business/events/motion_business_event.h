#ifndef __MOTION_BUSINESS_EVENT_H__
#define __MOTION_BUSINESS_EVENT_H__

#include <QtCore/QCoreApplication> //for Q_DECLARE_TR_FUNCTIONS

#include <business/business_resource_validator.h>
#include <business/events/prolonged_business_event.h>

#include <core/datapacket/abstract_data_packet.h>
#include <core/resource/resource_fwd.h>

class QnMotionBusinessEvent: public QnProlongedBusinessEvent
{
    typedef QnProlongedBusinessEvent base_type;
public:
    QnMotionBusinessEvent(const QnResourcePtr& resource, Qn::ToggleState toggleState, qint64 timeStamp, QnConstAbstractDataPacketPtr metadata);
private:
    QnConstAbstractDataPacketPtr m_metadata;
};

typedef QSharedPointer<QnMotionBusinessEvent> QnMotionBusinessEventPtr;

class QnCameraMotionAllowedPolicy {
    Q_DECLARE_TR_FUNCTIONS(QnCameraMotionAllowedPolicy)
public:
    typedef QnVirtualCameraResource resource_type;
    static inline bool emptyListIsValid() { return true; }
    static bool isResourceValid(const QnVirtualCameraResourcePtr &camera);
    static QString getErrorText(int invalid, int total);
};

typedef QnBusinessResourceValidator<QnCameraMotionAllowedPolicy> QnCameraMotionValidator;

#endif // __MOTION_BUSINESS_EVENT_H__
