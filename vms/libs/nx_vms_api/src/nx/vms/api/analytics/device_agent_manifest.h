// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QJsonObject>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/manifest_error.h>
#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/vms/api/analytics/type_library.h>

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
    NX_REFLECTION_ENUM_IN_CLASS(Capability,
        noCapabilities = 0,
        disableStreamSelection = 1 << 0,
        doNotSaveSettingsValuesToProperty = 1 << 31 /**< Proprietary. */
    )
    Q_DECLARE_FLAGS(Capabilities, Capability)

    Capabilities capabilities;

    /**%apidoc
     * %deprecated Use "supportedTypes" field instead.
     */
    QList<QString> supportedEventTypeIds;

    /**%apidoc
     * %deprecated Use "supportedTypes" field instead.
     */
    QList<QString> supportedObjectTypeIds;

    /**%apidoc
     * %deprecated Use "typeLibrary" and "supportedTypes" fields instead.
     */
    QList<EventType> eventTypes;

    /**%apidoc
     * %deprecated Use "typeLibrary" and "supportedTypes" fields instead.
     */
    QList<ObjectType> objectTypes;

    /**%apidoc
     * %deprecated Use "typeLibrary" field instead.
     */
    QList<Group> groups;

    QJsonValue deviceAgentSettingsModel; /**<%apidoc[proprietary] */

    QList<TypeSupportInfo> supportedTypes;
    TypeLibrary typeLibrary;
};

NX_VMS_API std::vector<ManifestError> validate(const DeviceAgentManifest& deviceAgentManifest);

#define DeviceAgentManifest_Fields \
    (capabilities) \
    (supportedEventTypeIds) \
    (supportedObjectTypeIds) \
    (eventTypes) \
    (objectTypes) \
    (groups) \
    (deviceAgentSettingsModel) \
    (supportedTypes) \
    (typeLibrary)

NX_REFLECTION_INSTRUMENT(DeviceAgentManifest, DeviceAgentManifest_Fields);

Q_DECLARE_OPERATORS_FOR_FLAGS(DeviceAgentManifest::Capabilities)

QN_FUSION_DECLARE_FUNCTIONS(DeviceAgentManifest, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics
