#include "business_event_widget_factory.h"

#include <ui/widgets/business/empty_business_event_widget.h>
#include <ui/widgets/business/motion_business_event_widget.h>
#include <ui/widgets/business/camera_input_business_event_widget.h>
#include <ui/widgets/business/camera_disconnected_business_event_widget.h>
#include <ui/widgets/business/storage_failure_business_event_widget.h>

QnAbstractBusinessParamsWidget* QnBusinessEventWidgetFactory::createWidget(BusinessEventType::Value eventType, QWidget *parent) {
    switch (eventType) {
    case BusinessEventType::BE_Camera_Motion:
        return new QnMotionBusinessEventWidget(parent);
    case BusinessEventType::BE_Camera_Input:
        return new QnCameraInputBusinessEventWidget(parent);
    case BusinessEventType::BE_Camera_Disconnect:
        return new QnCameraDisconnectedBusinessEventWidget(parent);
    case BusinessEventType::BE_Storage_Failure:
        return new QnStorageFailureBusinessEventWidget(parent);
    default:
        return new QnEmptyBusinessEventWidget(parent);
    }
}
