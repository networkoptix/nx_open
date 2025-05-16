// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_method_field.h"

#include <QtCore/QJsonArray>

#include <nx/network/http/http_types.h>
#include <nx/vms/rules/manifest.h>

namespace nx::vms::rules {

QSet<QString> HttpMethodField::allowedValues(bool allowAuto)
{
    QSet allowedValues = {
        QString(network::http::Method::get.data()),
        QString(network::http::Method::post.data()),
        QString(network::http::Method::put.data()),
        QString(network::http::Method::patch.data()),
        QString(network::http::Method::delete_.data())
    };

    if (allowAuto)
    {
        static const QString kAutoMethod; //< An empty value means that the method will be calculated automatically.
        allowedValues.insert(kAutoMethod);
    }

    return allowedValues;
}

QJsonObject HttpMethodField::openApiDescriptor(const QVariantMap& properties)
{
    auto descriptor = SimpleTypeActionField::openApiDescriptor({});
    QJsonArray enums;
    const auto allowed =
        allowedValues(HttpMethodFieldProperties::fromVariantMap(properties).allowAuto);
    for (const auto& value: allowed)
        enums.append(value);

    descriptor[utils::kEnumProperty] = enums;
    descriptor[utils::kExampleProperty] = QString(network::http::Method::post.data());
    return descriptor;
}

HttpMethodFieldProperties HttpMethodField::properties() const
{
    return HttpMethodFieldProperties::fromVariantMap(descriptor()->properties);
}

} // namespace nx::vms::rules
