// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"
#include "../data_macros.h"
#include "../manifest.h"

namespace nx::vms::rules {

using ListItem = QPair<QString, QString>;

struct ActionListFieldProperties
{
    QList<ListItem> items;

    static ActionListFieldProperties fromVariantMap(const QVariantMap& properties)
    {
        ActionListFieldProperties result;

        const auto itemsVariant = properties.value("items");
        const auto itemList = itemsVariant.toList();

        for (const auto& item : itemList)
        {
            if (item.typeId() != QMetaType::QVariantMap)
                continue;

            const auto map = item.toMap();
            const auto name = map.value("name");
            const auto value = map.value("value");

            if (name.isValid() && value.isValid())
                result.items.append(qMakePair(name.toString(), value.toString()));
        }
        return result;
    }
};

/**
 * Stores a string, selected from the list defined in properties. Displayed as a dropdown list.
 */
class NX_VMS_RULES_API StringSelectionField:
    public SimpleTypeActionField<QString, StringSelectionField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "stringSelection")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<QString, StringSelectionField>::SimpleTypeActionField;

    ActionListFieldProperties properties() const
    {
        return ActionListFieldProperties::fromVariantMap(descriptor()->properties);
    }

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
