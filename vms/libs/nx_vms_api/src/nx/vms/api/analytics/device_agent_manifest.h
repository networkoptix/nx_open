#pragma once

#include <QtCore/QJsonObject>

#include <vector>

#include <nx/vms/api/analytics/manifest_error.h>
#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

// TODO: Move this class to server-only library; eliminate its usages in the Client.
/**
 * The JSON-serializable data structure that is given by each Analytics Engine's DeviceAgent to the
 * Server after the DeviceAgent has been created by the Engine.
 *
 * See the description of the fields in manifests.md.
 */
struct NX_VMS_API DeviceAgentManifest
{
    enum Capability
    {
        noCapabilities = 0,
        disableStreamSelection = 1 << 0,
        doNotSaveSettingsValuesToProperty = 1 << 31, /**< Proprietary. */
    };
    Q_DECLARE_FLAGS(Capabilities, Capability);

    Capabilities capabilities;
    QList<QString> supportedEventTypeIds;
    QList<QString> supportedObjectTypeIds;
    QList<EventType> eventTypes;
    QList<ObjectType> objectTypes;
    QList<Group> groups;
    QJsonValue deviceAgentSettingsModel; /**< Proprietary, deprecated. */
};

NX_VMS_API std::vector<ManifestError> validate(const DeviceAgentManifest& deviceAgentManifest);

#define DeviceAgentManifest_Fields \
    (capabilities) \
    (supportedEventTypeIds) \
    (supportedObjectTypeIds) \
    (eventTypes) \
    (objectTypes) \
    (groups) \
    (deviceAgentSettingsModel)

Q_DECLARE_OPERATORS_FOR_FLAGS(DeviceAgentManifest::Capabilities)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(DeviceAgentManifest::Capability)
QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentManifest, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::DeviceAgentManifest::Capability,
    (metatype)(numeric)(lexical), NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::DeviceAgentManifest::Capabilities,
    (metatype)(numeric)(lexical), NX_VMS_API)
