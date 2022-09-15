// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "business_event_widget_factory.h"

#include <nx/vms/client/desktop/ui/event_rules/widgets/analytics_sdk_event_widget.h>
#include <nx/vms/client/desktop/ui/event_rules/widgets/analytics_sdk_object_detected_widget.h>
#include <nx/vms/client/desktop/ui/event_rules/widgets/plugin_diagnostic_event_widget.h>
#include <ui/widgets/business/camera_input_business_event_widget.h>
#include <ui/widgets/business/custom_business_event_widget.h>
#include <ui/widgets/business/empty_business_event_widget.h>
#include <ui/widgets/business/software_trigger_business_event_widget.h>
#include <nx/vms/client/desktop/system_context.h>

using namespace nx;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

QnAbstractBusinessParamsWidget* QnBusinessEventWidgetFactory::createWidget(
    nx::vms::api::EventType eventType,
    SystemContext* systemContext,
    QWidget* parent)
{
    switch (eventType)
    {
        case vms::api::EventType::cameraInputEvent:
            return new QnCameraInputBusinessEventWidget(systemContext, parent);
        case vms::api::EventType::softwareTriggerEvent:
            return new QnSoftwareTriggerBusinessEventWidget(systemContext, parent);
        case vms::api::EventType::userDefinedEvent:
            return new QnCustomBusinessEventWidget(systemContext, parent);
        case vms::api::EventType::analyticsSdkEvent:
            return new AnalyticsSdkEventWidget(systemContext, parent);
        case vms::api::EventType::analyticsSdkObjectDetected:
            return new AnalyticsSdkObjectDetectedWidget(systemContext, parent);
        case vms::api::EventType::pluginDiagnosticEvent:
            return new PluginDiagnosticEventWidget(systemContext, parent);
        default:
            return new QnEmptyBusinessEventWidget(systemContext, parent);
    }
}
