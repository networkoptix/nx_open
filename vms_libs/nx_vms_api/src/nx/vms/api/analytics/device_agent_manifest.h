#pragma once

#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API DeviceAgentManifest
{
    QList<QString> supportedEventTypeIds;
    QList<QString> supportedObjectTypeIds;

    QList<EventType> eventTypes;
    QList<ObjectType> objectTypes;
    QList<Group> groups;
};

#define DeviceAgentManifest_Fields \
    (supportedEventTypeIds)(supportedObjectTypeIds)(eventTypes)(objectTypes)(groups)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentManifest, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics
