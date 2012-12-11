#include "business_event_connector.h"
#include "motion_business_event.h"
#include "camera_input_business_event.h"
#include "core/resource/resource.h"
#include "business_rule_processor.h"

Q_GLOBAL_STATIC(QnBusinessEventConnector, static_instance)

QnBusinessEventConnector* QnBusinessEventConnector::instance()
{
    return static_instance();
}


void QnBusinessEventConnector::at_motionDetected(QnResourcePtr resource, bool value, qint64 timeStamp)
{
    QnMotionBusinessEventPtr motionEvent(new QnMotionBusinessEvent(
                                             resource->toSharedPointer(),
                                             value ? ToggleState::On : ToggleState::Off,
                                             timeStamp));
    bRuleProcessor->processBusinessEvent(motionEvent);
}

void QnBusinessEventConnector::at_cameraInput(
    QnResourcePtr resource,
    const QString& inputPortID,
    bool value,
    qint64 timeStamp )
{
    if( !resource )
        return;

    bRuleProcessor->processBusinessEvent(
        QnCameraInputEventPtr( new QnCameraInputEvent(
                                   resource->toSharedPointer(),
                                   value ? ToggleState::On : ToggleState::Off,
                                   timeStamp,
                                   inputPortID)));
}
