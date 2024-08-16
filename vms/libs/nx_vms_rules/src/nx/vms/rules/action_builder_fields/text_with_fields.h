// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/common/system_context_aware.h>

#include "../action_builder_field.h"

namespace nx::vms::rules {

constexpr auto kUrlValidationPolicy = "url";

struct TextWithFieldsFieldProperties
{
    /** Whether given field should be visible in the editor. */
    bool visible{true};

    QString validationPolicy;

    QVariantMap toVariantMap() const
    {
        QVariantMap result;

        result.insert("visible", visible);
        result.insert("validationPolicy", validationPolicy);

        return result;
    }

    static TextWithFieldsFieldProperties fromVariantMap(const QVariantMap& properties)
    {
        TextWithFieldsFieldProperties result;

        result.visible = properties.value("visible", true).toBool();
        result.validationPolicy = properties.value("validationPolicy").toString();

        return result;
    }
};

 /** Perform string formatting using event data values. */
class NX_VMS_RULES_API TextWithFields:
    public ActionBuilderField,
    public common::SystemContextAware
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.textWithFields")

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)

public:
    TextWithFields(common::SystemContext* context, const FieldDescriptor* descriptor);
    virtual ~TextWithFields() override;

    virtual QVariant build(const AggregatedEventPtr& eventAggregator) const override;

    QString text() const;
    void setText(const QString& text);

    TextWithFieldsFieldProperties properties() const;

    void parseText();

    enum class FieldType
    {
        Text,
        Substitution
    };

    struct ValueDescriptor
    {
        FieldType type = FieldType::Text;
        QString value;
        bool isValid = true;
        qsizetype start = 0;
        qsizetype length = 0;
    };

    using ParsedValues = QList<ValueDescriptor>;

    const ParsedValues& parsedValues() const;
signals:
    void textChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

/** Same as base class but hidden from user. */
class NX_VMS_RULES_API TextFormatter: public TextWithFields
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.textFormatter")

public:
    using TextWithFields::TextWithFields;
};

} // namespace nx::vms::rules
