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
 */
struct NX_VMS_API DeviceAgentManifest
{
    enum Capability
    {
        noCapabilities = 0,
        disableStreamSelection = 1 << 0,
        doNotSaveSettingsValuesToProperty = 1 << 31, //< Properietary flag.
    };
    Q_DECLARE_FLAGS(Capabilities, Capability);

    Capabilities capabilities;

    /** Whitelist (filter) of Event types for types declared in the owner Engine manifest. */
    QList<QString> supportedEventTypeIds;

    /** Whitelist (filter) of Object types for types declared in the owner Engine manifest. */
    QList<QString> supportedObjectTypeIds;

    /**
     * Types of Events that can be generated by this particular DeviceAgent instance, in addition
     * to the types declared in the Engine manifest.
     */
    QList<EventType> eventTypes;

    /**
     * Types of Objects that can be generated by this particular DeviceAgent instance, in addition
     * to the types declared in the Engine manifest.
     */
    QList<ObjectType> objectTypes;

    /** Groups used to group Object and Event types declared by this manifest. */
    QList<Group> groups;

    /** Settings model that overrides the one declared in the Engine manifest. */
    QJsonValue deviceAgentSettingsModel;
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
