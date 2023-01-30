// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/latin1_array.h>
#include <nx/vms/api/types/access_rights_types.h>

#include "field.h"

namespace nx::vms::rules {

NX_REFLECTION_ENUM_CLASS(ItemFlag,
    instant = 1 << 0,
    prolonged = 1 << 1
)

Q_DECLARE_FLAGS(ItemFlags, ItemFlag)

struct ResourcePermission
{
    QnLatin1Array fieldName;
    Qn::Permissions permissions;
};
#define nx_vms_rules_ResourcePermission_Fields (fieldName)(permissions)
NX_VMS_RULES_API void serialize(
    QnJsonContext* ctx, const ResourcePermission& value, QJsonValue* target);

struct PermissionsDescriptor
{
    nx::vms::api::GlobalPermission globalPermission = nx::vms::api::GlobalPermission::none;
    QList<ResourcePermission> resourcePermissions;
};
#define nx_vms_rules_PermissionsDescriptor_Fields \
    (globalPermission)(resourcePermissions)
NX_VMS_RULES_API void serialize(
    QnJsonContext* ctx, const PermissionsDescriptor& value, QJsonValue* target);

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

    /**%apidoc Field description to show hint to the user. */
    QString description;

    /**%apidoc:object Optional properties corresponding to the actual field id. */
    QVariantMap properties;

    /**%apidoc Field names of the parent item that required for the current field. */
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

    /**%apidoc Display name. */
    QString displayName;

    /**%apidoc Item description, to show hint to the user. */
    QString description;

    ItemFlags flags = ItemFlag::instant;

    /**%apidoc Item fields. */
    QList<FieldDescriptor> fields;

    /**%apidoc Permissions required for action recipient. */
    PermissionsDescriptor permissions;

    /**%apidoc Path to the mustache template file used to generate email. */
    QString emailTemplatePath; // TODO: #mmalofeev split ItemDescriptor to EventDescriptior and ActionDescriptor.
};
#define nx_vms_rules_ItemDescriptor_Fields \
    (id)(displayName)(description)(flags)(fields)(permissions)(emailTemplatePath)
NX_VMS_RULES_API void serialize(
    QnJsonContext* ctx, const ItemDescriptor& value, QJsonValue* target);

template<class T>
FieldDescriptor makeFieldDescriptor(
    const QString& fieldName,
    const QString& displayName,
    const QString& description = {},
    const QVariantMap& properties = {},
    const QStringList& linkedFields = {})
{
    return FieldDescriptor{
        .id = fieldMetatype<T>(),
        .fieldName = fieldName,
        .displayName = displayName,
        .description = description,
        .properties = properties,
        .linkedFields = linkedFields,
    };
}

} // namespace nx::vms::rules
