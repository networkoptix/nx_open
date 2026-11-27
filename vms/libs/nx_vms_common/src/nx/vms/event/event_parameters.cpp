// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_parameters.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/json.h>

namespace nx {
namespace vms {
namespace event {

QString EventParameters::getAnalyticsEventTypeId() const
{
    return inputPortId;
}

void EventParameters::setAnalyticsEventTypeId(const QString& id)
{
    inputPortId = id;
}

QString EventParameters::getAnalyticsObjectTypeId() const
{
    return inputPortId;
}

void EventParameters::setAnalyticsObjectTypeId(const QString& id)
{
    inputPortId = id;
}

nx::Uuid EventParameters::getAnalyticsEngineId() const
{
    return analyticsEngineId;
}

void EventParameters::setAnalyticsEngineId(const nx::Uuid& id)
{
    analyticsEngineId = id;
}

QString EventParameters::getTriggerName() const
{
    return caption;
}

void EventParameters::setTriggerName(const QString& name)
{
    caption = name;
}

QString EventParameters::getTriggerIcon() const
{
    return description;
}

void EventParameters::setTriggerIcon(const QString& icon)
{
    description = icon;
}

nx::vms::api::EventLevels EventParameters::getDiagnosticEventLevels() const
{
    return nx::reflect::fromString<nx::vms::api::EventLevels>(inputPortId.toStdString());
}

void EventParameters::setDiagnosticEventLevels(nx::vms::api::EventLevels levels)
{
    inputPortId = QString::fromStdString(nx::reflect::toString(levels));
}

bool EventParameters::canHaveVideoLink() const
{
    return eventType == EventType::cameraMotionEvent
        || eventType == EventType::cameraInputEvent
        || eventType == EventType::analyticsSdkEvent
        || eventType == EventType::analyticsSdkObjectDetected
        || eventType == EventType::userDefinedEvent;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EventMetaData, (ubjson)(json), EventMetaData_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    EventParameters, (ubjson)(json), EventParameters_Fields, (brief, true))

} // namespace event
} // namespace vms
} // namespace nx
