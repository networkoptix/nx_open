// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <common/common_globals.h>
#include <nx/vms/api/types/access_rights_types.h>

#include "field.h"

namespace nx::vms::rules {

enum class ItemFlag
{
    instant = 1 << 0,
    prolonged = 1 << 1,
};

Q_DECLARE_FLAGS(ItemFlags, ItemFlag)

struct ResourcePermission
{
    QByteArray fieldName;
    Qn::Permissions permissions;
};

struct PermissionsDescriptor
{
    nx::vms::api::GlobalPermission globalPermission = nx::vms::api::GlobalPermission::none;
    QList<ResourcePermission> resourcePermissions;
};

/** Description of event or action field. */
struct FieldDescriptor
{
    /** Field unique id. */
    QString id;

    /**
     * Field name to find the corresponding data.
     *
     * We need to now how to fetch the corresponding data from the event filter or action.
     * builder. This property gives us an opportunity to match the rule data to editor fields.
     */
    QString fieldName;

    /** Field display name. */
    QString displayName;

    /** Field description to show hint to the user. */
    QString description;

    /** Optional properties corresponding to the actual field id. */
    QVariantMap properties;

    /** Field names of the parent item that required for the current field. */
    QStringList linkedFields;
};

/** Description of event or action with default field set. */
struct ItemDescriptor
{
    /** Item unique id. */
    QString id;

    /** Display name. */
    QString displayName;

    /** Item description, to show hint to the user. */
    QString description;

    ItemFlags flags = ItemFlag::instant;

    /** Item fields. */
    QList<FieldDescriptor> fields;

    /** Permissions required for action recipient. */
    PermissionsDescriptor permissions;

    /** Path to the mustache template file used to generate email. */
    QString emailTemplatePath; // TODO: #mmalofeev split ItemDescriptor to EventDescriptior and ActionDescriptor.
};

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
