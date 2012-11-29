#include "business_event_connector.h"
#include "motion_business_event.h"
#include "camera_input_event.h"
#include "core/resource/resource.h"
#include "business_rule_processor.h"

Q_GLOBAL_STATIC(QnBusinessEventConnector, static_instance)

QnBusinessEventConnector* QnBusinessEventConnector::instance()
{
    return static_instance();
}


void QnBusinessEventConnector::at_motionDetected(QnResourcePtr resource, bool value, qint64 timestamp)
{
    QnMotionBusinessEventPtr motionEvent(new QnMotionBusinessEvent());
    motionEvent->setToggleState(value ? ToggleState_On : ToggleState_Off);
    motionEvent->setDateTime(timestamp);
    motionEvent->setResource(resource->toSharedPointer());

    bRuleProcessor->processBusinessEvent(motionEvent);
}

void QnBusinessEventConnector::at_cameraInput(
    QnResourcePtr resource,
    const QString& inputPortID,
    bool value,
    qint64 timestamp )
{
    if( !resource )
        return;

    bRuleProcessor->processBusinessEvent(
        QnCameraInputEventPtr( new QnCameraInputEvent(
            resource->toSharedPointer(),
            inputPortID,
            value ? ToggleState_On : ToggleState_Off,
            timestamp ) ) );
}
