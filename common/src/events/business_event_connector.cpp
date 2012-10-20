#include "business_event_connector.h"
#include "motion_business_event.h"
#include "core/resource/resource.h"

Q_GLOBAL_STATIC(QnBusinessEventConnector, static_instance)

QnBusinessEventConnector* QnBusinessEventConnector::instance()
{
    return static_instance();
}


void QnBusinessEventConnector::at_motionDetected(bool value, qint64 timestamp)
{
    QnResource* resource = dynamic_cast<QnResource*>(sender());
    Q_ASSERT(resource);
    QnMotionBusinessEventPtr motionEvent(new QnMotionBusinessEvent());
    motionEvent->setToggleState(value ? QnToggleBusinessEvent::On : QnToggleBusinessEvent::Off);
    motionEvent->setDateTime(timestamp);
    motionEvent->setResource(resource->toSharedPointer());
}
