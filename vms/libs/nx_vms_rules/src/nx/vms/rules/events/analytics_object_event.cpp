// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_object_event.h"

#include "../event_fields/analytics_object_attributes_field.h"
#include "../event_fields/analytics_object_type_field.h"
#include "../event_fields/source_camera_field.h"
#include "../utils/type.h"

namespace nx::vms::rules {

AnalyticsObjectEvent::AnalyticsObjectEvent(
    std::chrono::microseconds timestamp,
    QnUuid cameraId,
    QnUuid engineId,
    const QString& objectTypeId,
    QnUuid objectTrackId,
    const nx::common::metadata::Attributes& attributes)
    :
    BasicEvent(timestamp),
    m_cameraId(cameraId),
    m_engineId(engineId),
    m_objectTypeId(objectTypeId),
    m_objectTrackId(objectTrackId),
    m_attributes(attributes)
{
}

const ItemDescriptor& AnalyticsObjectEvent::manifest()
{
    static const auto kDescriptor = ItemDescriptor{
        .id = utils::type<AnalyticsObjectEvent>(),
        .displayName = tr("Analytics Object Detected"),
        .description = {},
        .fields = {
            {fieldMetatype<SourceCameraField>(), "cameraId", tr("Camera")},
            {fieldMetatype<AnalyticsObjectTypeField>(), "objectTypeId", tr("Object Type")},
            {fieldMetatype<AnalyticsObjectAttributesField>(), "attributes", tr("Attributes")},
        }
    };

    return kDescriptor;
}

} // namespace nx::vms::rules
