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

    /**
    * Separate action copies with different resources (filtered according to user permissions)
    * may be created for different target users. One action copy without any filtration,
    * containing all of the resources and no target users may be needed for server processing.
    * The flag marks actions requiring such separate copy.
    */
    executeOnClientAndServer = 1 << 2,

    /**
     * The default way to aggregate events is to use the EventPtr::aggregationKey function.
     * If both the event and the action have the flag, more generic aggregation
     * must be used by the event type.
     */
    aggregationByTypeSupported = 1 << 3,
    omitLogging = 1 << 4,
    /** The action may be displayed in event log, but not in rule editor. */
    system = 1 << 5,
    /** The item may appear in the event log, but must not be created. */
    deprecated = 1 << 6
)

Q_DECLARE_FLAGS(ItemFlags, ItemFlag)

NX_REFLECTION_ENUM_CLASS(ResourceType,
    None,
    Device,
    Server,
    Layout,
    AnalyticsEngine,
    User
)

/** Description of event or action resource data field. */
struct ResourceDescriptor
{
    /**%apidoc The type of the resource. */
    ResourceType type = ResourceType::None;

    /**%apidoc[opt] Permissions required to view the item. */
    Qn::Permissions readPermissions = Qn::ReadPermission;

    /**%apidoc[opt] Permissions required to create an item. */
    Qn::Permissions createPermissions = Qn::UserInputPermissions;
};

#define nx_vms_rules_ResourceDescriptor_Fields \
    (type)(readPermissions)(createPermissions)
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
    QVariantMap properties;

    /**%apidoc[opt] Field names of the parent item that required for the current field. */
    QStringList linkedFields;
};
#define nx_vms_rules_FieldDescriptor_Fields \
    (id)(fieldName)(displayName)(description)(properties)(linkedFields)
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
    (id)(groupId)(displayName)(description)(flags)(fields)(resources) \
    (readPermissions)(createPermissions)(emailTemplatePath)(serverFlags)
NX_VMS_RULES_API void serialize(
    QnJsonContext* ctx, const ItemDescriptor& value, QJsonValue* target);

} // namespace nx::vms::rules
