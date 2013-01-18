#include "business_event_widget_factory.h"

#include <ui/widgets/business/empty_business_event_widget.h>
#include <ui/widgets/business/motion_business_event_widget.h>
#include <ui/widgets/business/camera_input_business_event_widget.h>

QnAbstractBusinessParamsWidget* QnBusinessEventWidgetFactory::createWidget(BusinessEventType::Value eventType, QWidget *parent,
                                                                           QnWorkbenchContext *context) {
    Q_UNUSED(context);
    switch (eventType) {
    case BusinessEventType::BE_Camera_Input:
        return new QnCameraInputBusinessEventWidget(parent);
    default:
        return new QnEmptyBusinessEventWidget(parent);
    }
}
