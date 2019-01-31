#include "business_event_widget_factory.h"

#include <ui/widgets/business/empty_business_event_widget.h>
#include <ui/widgets/business/camera_input_business_event_widget.h>
#include <ui/widgets/business/software_trigger_business_event_widget.h>
#include <ui/widgets/business/custom_business_event_widget.h>
#include <nx/vms/client/desktop/ui/event_rules/widgets/analytics_sdk_event_widget.h>
#include <nx/vms/client/desktop/ui/event_rules/widgets/plugin_event_widget.h>

using namespace nx;
using namespace nx::vms::client::desktop::ui;

QnAbstractBusinessParamsWidget* QnBusinessEventWidgetFactory::createWidget(
    vms::api::EventType eventType, QWidget* parent,
    QnWorkbenchContext* /*context*/)
{
    switch (eventType)
    {
        case vms::api::EventType::cameraInputEvent:
            return new QnCameraInputBusinessEventWidget(parent);
        case vms::api::EventType::softwareTriggerEvent:
            return new QnSoftwareTriggerBusinessEventWidget(parent);
        case vms::api::EventType::userDefinedEvent:
            return new QnCustomBusinessEventWidget(parent);
        case vms::api::EventType::analyticsSdkEvent:
            return new AnalyticsSdkEventWidget(parent);
        case vms::api::EventType::pluginEvent:
            return new PluginEventWidget(parent);
        default:
            return new QnEmptyBusinessEventWidget(parent);
    }
}
