// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"
#include "../manifest.h"

namespace nx::vms::rules {

struct ActionTextFieldProperties
{
    bool allowEmptyText{true};
    QString validationPolicy;

    QVariantMap toVariantMap() const
    {
        QVariantMap result;

        result.insert("allowEmptyText", allowEmptyText);
        result.insert("validationPolicy", validationPolicy);

        return result;
    }

    static ActionTextFieldProperties fromVariantMap(const QVariantMap& properties)
    {
        ActionTextFieldProperties result;

        result.allowEmptyText = properties.value("allowEmptyText", true).toBool();
        result.validationPolicy = properties.value("validationPolicy").toString();

        return result;
    }
};

constexpr auto kEmailsValidationPolicy = "emails";

/** Stores simple text string w/o validation, e.g. email, login or comment. */
class NX_VMS_RULES_API ActionTextField: public SimpleTypeActionField<QString, ActionTextField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "text")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<QString, ActionTextField>::SimpleTypeActionField;

    ActionTextFieldProperties properties() const
    {
        return ActionTextFieldProperties::fromVariantMap(descriptor()->properties);
    }

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
