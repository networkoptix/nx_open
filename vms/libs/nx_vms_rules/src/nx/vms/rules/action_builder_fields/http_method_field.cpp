// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_method_field.h"

#include <QtCore/QJsonArray>

#include <nx/network/http/http_types.h>

namespace nx::vms::rules {

const QSet<QString>& HttpMethodField::allowedValues()
{
    static const QSet kAllowedValues = {QString(network::http::Method::get.data()),
        QString(network::http::Method::post.data()),
        QString(network::http::Method::put.data()),
        QString(network::http::Method::patch.data()),
        QString(network::http::Method::delete_.data())};

    return kAllowedValues;
}

QJsonObject HttpMethodField::openApiDescriptor(const QVariantMap&)
{
    auto descriptor = SimpleTypeActionField::openApiDescriptor({});
    QJsonArray enums;
    for (const auto& value: allowedValues())
        enums.append(value);
    descriptor[utils::kEnumProperty] = enums;
    descriptor[utils::kExampleProperty] = *allowedValues().begin();
    return descriptor;
}

} // namespace nx::vms::rules
