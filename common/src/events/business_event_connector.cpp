#include "business_event_connector.h"
#include "motion_business_event.h"
#include "camera_input_business_event.h"
#include "core/resource/resource.h"
#include "business_rule_processor.h"
#include "camera_disconnected_business_event.h"
#include "storage_failure_business_event.h"

Q_GLOBAL_STATIC(QnBusinessEventConnector, static_instance)

QnBusinessEventConnector* QnBusinessEventConnector::instance()
{
    return static_instance();
}

void QnBusinessEventConnector::at_motionDetected(const QnResourcePtr &resource, bool value, qint64 timeStamp, QnMetaDataV1Ptr metadata)
{
    QnMotionBusinessEventPtr motionEvent(new QnMotionBusinessEvent(
                                             resource,
                                             value ? ToggleState::On : ToggleState::Off,
                                             timeStamp,
                                             metadata));
    qnBusinessRuleProcessor->processBusinessEvent(motionEvent);
}

void QnBusinessEventConnector::at_cameraDisconnected(const QnResourcePtr &resource, qint64 timeStamp)
{
    QnCameraDisconnectedBusinessEventPtr cameraEvent(new QnCameraDisconnectedBusinessEvent(
        resource->getParentResource(),
        resource,
        timeStamp));
    qnBusinessRuleProcessor->processBusinessEvent(cameraEvent);
}

void QnBusinessEventConnector::at_storageFailure(const QnResourcePtr &resource, qint64 timeStamp, const QString& reason)
{
    QnStorageFailureBusinessEventPtr cameraEvent(new QnStorageFailureBusinessEvent(
        resource,
        timeStamp,
        reason));
    qnBusinessRuleProcessor->processBusinessEvent(cameraEvent);
}

void QnBusinessEventConnector::at_cameraInput(const QnResourcePtr &resource, const QString& inputPortID, bool value, qint64 timeStamp)
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
