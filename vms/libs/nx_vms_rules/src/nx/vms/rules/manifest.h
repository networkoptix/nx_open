// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QVariant>

#include "field.h"

namespace nx::vms::rules {

enum class ItemFlag
{
    instant = 1 << 0,
    prolonged = 1 << 1,
};

Q_DECLARE_FLAGS(ItemFlags, ItemFlag)

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
};

template<class T>
FieldDescriptor makeFieldDescriptor(
    const QString& fieldName,
    const QString& displayName,
    const QString& description = {},
    const QVariantMap& properties = {})
{
    return FieldDescriptor{
        .id = fieldMetatype<T>(),
        .fieldName = fieldName,
        .displayName = displayName,
        .description = description,
        .properties = properties,
    };
}

} // namespace nx::vms::rules
