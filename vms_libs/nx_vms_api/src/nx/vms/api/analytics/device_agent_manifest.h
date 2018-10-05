#pragma once

#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API DeviceAgentManifest
{
    QList<QString> supportedEventTypes; //< TODO: Rename (including db) to supportedEventTypeIds.
    QList<QString> supportedObjectTypes; //< TODO: Rename (including db) to supportedObjectTypeIds.

    QList<EventType> outputEventTypes; //< TODO: Rename (including db) to eventTypes.
    QList<ObjectType> outputObjectTypes; //< TODO: Rename (including db) to objectTypes.
    QList<Group> groups;
};

#define DeviceAgentManifest_Fields \
    (supportedEventTypes)(supportedObjectTypes)(outputEventTypes)(outputObjectTypes)(groups)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentManifest, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics
