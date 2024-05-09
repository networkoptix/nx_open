// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/latin1_array.h>
#include <nx/vms/api/types/access_rights_types.h>
#include <nx/vms/api/types/resource_types.h>

namespace nx::vms::rules {

NX_REFLECTION_ENUM_CLASS(ItemFlag,
    instant = 1 << 0,
    prolonged = 1 << 1,

    /** The action may be displayed in the event log, but not in the rule editor. */
    system = 1 << 2,

    /**
     * The default way to aggregate events is to use the EventPtr::aggregationKey function.
     * If both the event and the action have the flag, more generic aggregation
     * must be used by the event type.
     */
    aggregationByTypeSupported = 1 << 3,
    omitLogging = 1 << 4,

    /** The item may appear in the event log, but must not be created. */
    deprecated = 1 << 5
)

Q_DECLARE_FLAGS(ItemFlags, ItemFlag)

NX_REFLECTION_ENUM_CLASS(ExecutionTarget,
    clients = 1 << 0,
    servers = 1 << 1,
    cloud = 1 << 2
)

Q_DECLARE_FLAGS(ExecutionTargets, ExecutionTarget)

NX_REFLECTION_ENUM_CLASS(TargetServers,
    currentServer,
    resourceOwner,
    serverWithPublicIp
)

NX_REFLECTION_ENUM_CLASS(ResourceType,
    none,
    device,
    server,
    layout,
    analyticsEngine,
    user
)

NX_REFLECTION_ENUM_CLASS(FieldFlag,
    none,
    /** Allows absence of the resource. */
    optional = 1 << 0,
    /** Target resource for action routing. */
    target = 1 << 1
)

Q_DECLARE_FLAGS(FieldFlags, FieldFlag)

/** Description of event or action resource data field. */
struct ResourceDescriptor
{
    /**%apidoc The type of the resource. */
    ResourceType type = ResourceType::none;

    /**%apidoc[opt] Permissions required to view the item. */
    Qn::Permissions readPermissions;

    /**%apidoc[opt] Permissions required to create an item. */
    Qn::Permissions createPermissions = Qn::UserInputPermissions;

    /**%apidoc[opt] Resource requirements and processing flags. */
    FieldFlags flags;
};

#define nx_vms_rules_ResourceDescriptor_Fields \
    (type)(readPermissions)(createPermissions)(flags)
NX_VMS_RULES_API void serialize(
    QnJsonContext* ctx, const ResourceDescriptor& value, QJsonValue* target);

/** Description of event or action field. */
struct FieldDescriptor
{
    /**%apidoc Field unique id. */
    QString id;

    /**%apidoc
     * Field name to find the corresponding data.
     * We need to now how to fetch the corresponding data from the event filter or action.
     * builder. This property gives us an opportunity to match the rule data to editor fields.
     */
    QString fieldName;

    /**%apidoc Field display name. */
    QString displayName;

    /**%apidoc[opt] Field description to show hint to the user. */
    QString description;

    /**%apidoc[opt]:object Optional properties corresponding to the actual field id. */
    QVariantMap properties; //< TODO: #mmalofeev should properties required for the field initialisation be separated from the supportive properties?
};
#define nx_vms_rules_FieldDescriptor_Fields (id)(fieldName)(displayName)(description)(properties)

NX_VMS_RULES_API void serialize(
    QnJsonContext* ctx, const FieldDescriptor& value, QJsonValue* target);

/** Description of event or action with default field set. */
struct ItemDescriptor
{
    /**%apidoc Item unique id. */
    QString id;

    /**%apidoc[opt] Item group unique id. Empty value is root group. */
    std::string groupId;

    /**%apidoc Display name. */
    QString displayName;

    /**%apidoc[opt] Item description, to show hint to the user. */
    QString description;

    /**%apidoc Item flags. */
    ItemFlags flags = ItemFlag::instant;

    ExecutionTargets executionTargets = ExecutionTarget::servers;

    TargetServers targetServers = TargetServers::currentServer;

    /**%apidoc The list of event filter fields or action builder fields. */
    QList<FieldDescriptor> fields;

    /**%apidoc Resource descriptors of the item data fields. */
    std::map<std::string, ResourceDescriptor> resources;

    /**%apidoc[opt] Global permissions required to view the item. */
    nx::vms::api::GlobalPermissions readPermissions;

    /**%apidoc[opt] Global permissions required to create an item. */
    nx::vms::api::GlobalPermissions createPermissions;

    // TODO: #amalov Consider unifying fields, permissions and resources members.

    /**%apidoc Path to the mustache template file used to generate email. */
    QString emailTemplatePath; // TODO: #mmalofeev split ItemDescriptor to EventDescriptior and ActionDescriptor.

    /**%apidoc[opt]
     * Required Server flags on at least one Server of the merged ones to deal with such an event
     * or action.
     */
    vms::api::ServerFlags serverFlags;
};
#define nx_vms_rules_ItemDescriptor_Fields \
    (id)(groupId)(displayName)(description)(flags)(executionTargets)(targetServers)(fields) \
    (resources)(readPermissions)(createPermissions)(emailTemplatePath)(serverFlags)
NX_VMS_RULES_API void serialize(
    QnJsonContext* ctx, const ItemDescriptor& value, QJsonValue* target);

} // namespace nx::vms::rules
