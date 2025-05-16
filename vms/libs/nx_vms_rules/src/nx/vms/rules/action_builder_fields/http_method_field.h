// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

struct HttpMethodFieldProperties
{
    QString value;

    /** Whether auto runtime http method selection allowed. */
    bool allowAuto{true};

    QVariantMap toVariantMap() const
    {
        QVariantMap result;

        result.insert("value", value);
        result.insert("allowAuto", allowAuto);

        return result;
    }

    static HttpMethodFieldProperties fromVariantMap(const QVariantMap& properties)
    {
        HttpMethodFieldProperties result;

        result.value = properties.value("value").toString();
        result.allowAuto = properties.value("allowAuto", true).toBool();

        return result;
    }
};

/** Stores HTTP method as a string. */
class NX_VMS_RULES_API HttpMethodField: public SimpleTypeActionField<QString, HttpMethodField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "httpMethod")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<QString, HttpMethodField>::SimpleTypeActionField;
    static QSet<QString> allowedValues(bool allowAuto);
    static QJsonObject openApiDescriptor(const QVariantMap& properties);

    HttpMethodFieldProperties properties() const;

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
