#include "business_event_connector.h"
#include "motion_business_event.h"
#include "camera_input_business_event.h"
#include "core/resource/resource.h"
#include "business_rule_processor.h"
#include "camera_disconnected_business_event.h"

Q_GLOBAL_STATIC(QnBusinessEventConnector, static_instance)

QnBusinessEventConnector* QnBusinessEventConnector::instance()
{
    return static_instance();
}


void QnBusinessEventConnector::at_motionDetected(QnResourcePtr resource, bool value, qint64 timeStamp, QnMetaDataV1Ptr metadata)
{
    QnMotionBusinessEventPtr motionEvent(new QnMotionBusinessEvent(
                                             resource,
                                             value ? ToggleState::On : ToggleState::Off,
                                             timeStamp,
                                             metadata));
    qnBusinessRuleProcessor->processBusinessEvent(motionEvent);
}

void QnBusinessEventConnector::at_cameraDisconnected(QnResourcePtr cameraResource, qint64 timeStamp)
{
    QnCameraDisconnectedBusinessEventPtr cameraEvent(new QnCameraDisconnectedBusinessEvent(
        cameraResource->getParentResource(),
        cameraResource,
        timeStamp));
    qnBusinessRuleProcessor->processBusinessEvent(cameraEvent);
}

void QnBusinessEventConnector::at_cameraInput(
    QnResourcePtr resource,
    const QString& inputPortID,
    bool value,
    qint64 timeStamp )
{
    if( !resource )
        return;

    qnBusinessRuleProcessor->processBusinessEvent(
        QnCameraInputEventPtr( new QnCameraInputEvent(
                                   resource->toSharedPointer(),
                                   value ? ToggleState::On : ToggleState::Off,
                                   timeStamp,
                                   inputPortID)));
}
