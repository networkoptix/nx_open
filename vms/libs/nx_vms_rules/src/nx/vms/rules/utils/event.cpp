// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event.h"

#include "../manifest.h"
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

QnUuid sourceId(const BasicEvent* event)
{
    const auto getId =
        [event](const char* propName)
        {
            return event->property(propName).value<QnUuid>();
        };

    if (const auto cameraId = getId(utils::kCameraIdFieldName); !cameraId.isNull())
        return cameraId;

    if (const auto serverId = getId(utils::kServerIdFieldName); !serverId.isNull())
        return serverId;

    return {};
}

} // namespace nx::vms::rules
