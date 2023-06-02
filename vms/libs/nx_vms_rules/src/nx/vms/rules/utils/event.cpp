// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event.h"

#include "../basic_event.h"
#include "../engine.h"
#include "../manifest.h"
#include "common.h"
#include "field.h"

namespace nx::vms::rules {

bool hasSourceCamera(const vms::rules::ItemDescriptor& eventDescriptor)
{
    return std::any_of(
        eventDescriptor.fields.begin(),
        eventDescriptor.fields.end(),
        [](const vms::rules::FieldDescriptor& fieldDescriptor)
        {
            return fieldDescriptor.fieldName == vms::rules::utils::kDeviceIdsFieldName
                || fieldDescriptor.fieldName == vms::rules::utils::kCameraIdFieldName;
        });
}

bool hasSourceServer(const vms::rules::ItemDescriptor& eventDescriptor)
{
    return std::any_of(
        eventDescriptor.fields.begin(),
        eventDescriptor.fields.end(),
        [](const vms::rules::FieldDescriptor& fieldDescriptor)
        {
            return fieldDescriptor.fieldName == vms::rules::utils::kServerIdFieldName;
        });
}

bool isLoggingAllowed(const Engine* engine, const EventPtr& event)
{
    const auto descriptor = engine->eventDescriptor(event->type());
    if (!NX_ASSERT(descriptor))
        return false;

    return utils::isLoggingAllowed(descriptor.value(), event);
}

} // namespace nx::vms::rules
