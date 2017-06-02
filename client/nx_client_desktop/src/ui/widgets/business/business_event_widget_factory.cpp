#include "business_event_widget_factory.h"

#include <ui/widgets/business/empty_business_event_widget.h>
#include <ui/widgets/business/camera_input_business_event_widget.h>
#include <ui/widgets/business/software_trigger_business_event_widget.h>
#include <ui/widgets/business/custom_business_event_widget.h>

using namespace nx;

QnAbstractBusinessParamsWidget* QnBusinessEventWidgetFactory::createWidget(
    vms::event::EventType eventType, QWidget* parent,
    QnWorkbenchContext* /*context*/)
{
    switch (eventType)
    {
        case vms::event::CameraInputEvent:
            return new QnCameraInputBusinessEventWidget(parent);
        case vms::event::SoftwareTriggerEvent:
            return new QnSoftwareTriggerBusinessEventWidget(parent);
        case vms::event::UserDefinedEvent:
            return new QnCustomBusinessEventWidget(parent);
        default:
            return new QnEmptyBusinessEventWidget(parent);
    }
}
