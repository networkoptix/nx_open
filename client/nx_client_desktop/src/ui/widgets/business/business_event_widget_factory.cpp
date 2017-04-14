#include "business_event_widget_factory.h"

#include <ui/widgets/business/empty_business_event_widget.h>
#include <ui/widgets/business/camera_input_business_event_widget.h>
#include <ui/widgets/business/software_trigger_business_event_widget.h>
#include <ui/widgets/business/custom_business_event_widget.h>

QnAbstractBusinessParamsWidget* QnBusinessEventWidgetFactory::createWidget(QnBusiness::EventType eventType, QWidget *parent,
                                                                           QnWorkbenchContext *context) {
    Q_UNUSED(context);
    switch (eventType) {
    case QnBusiness::CameraInputEvent:
        return new QnCameraInputBusinessEventWidget(parent);
    case QnBusiness::SoftwareTriggerEvent:
        return new QnSoftwareTriggerBusinessEventWidget(parent);
    case QnBusiness::UserDefinedEvent:
        return new QnCustomBusinessEventWidget(parent);
    default:
        return new QnEmptyBusinessEventWidget(parent);
    }
}
