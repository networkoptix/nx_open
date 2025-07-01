// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/common/system_context_aware.h>
#include <nx/vms/rules/utils/text_tokenizer.h>

#include "../action_builder_field.h"

namespace nx::vms::rules {

constexpr auto kUrlValidationPolicy = "url";
constexpr auto kSiteEndpointValidationPolicy = "siteEndpoint";

struct TextWithFieldsFieldProperties
{
    FieldProperties base;

    QString validationPolicy;

    /** Whether incorrect substitutions must be highlighted. */
    bool highlightErrors{true};

    QVariantMap toVariantMap() const
    {
        QVariantMap result = base.toVariantMap();

        result.insert("validationPolicy", validationPolicy);
        result.insert("highlightErrors", highlightErrors);

        return result;
    }

    static TextWithFieldsFieldProperties fromVariantMap(const QVariantMap& properties)
    {
        TextWithFieldsFieldProperties result;

        result.base = FieldProperties::fromVariantMap(properties);
        result.validationPolicy = properties.value("validationPolicy").toString();
        result.highlightErrors =
            properties.value("highlightErrors", result.highlightErrors).toBool();

        return result;
    }
};

 /** Perform string formatting using event data values. */
class NX_VMS_RULES_API TextWithFields:
    public ActionBuilderField,
    public common::SystemContextAware
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "textWithFields")

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)

public:
    TextWithFields(common::SystemContext* context, const FieldDescriptor* descriptor);
    virtual ~TextWithFields() override;

    virtual QVariant build(const AggregatedEventPtr& event) const override;

    QString text() const;
    void setText(const QString& text);

    static QJsonObject openApiDescriptor(const QVariantMap& properties);

    TextWithFieldsFieldProperties properties() const;

    void parseText();

    const utils::TextTokenList& parsedValues() const;
signals:
    void textChanged();

private:
    QString m_rawText;
    utils::TextTokenList m_tokenizedText;
};

} // namespace nx::vms::rules
