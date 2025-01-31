// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_headers_field.h"

#include <nx/vms/rules/utils/openapi_doc.h>

namespace nx::vms::rules {

QJsonObject HttpHeadersField::openApiDescriptor(const QVariantMap& properties)
{
    auto descriptor =
        SimpleTypeActionField<QList<KeyValueObject>, HttpHeadersField>::openApiDescriptor(properties);
    descriptor[utils::kExampleProperty] =
        QJsonArray{R"({"key": "headerName", "value": "headerValue"})"};
    descriptor[utils::kDescriptionProperty] = "Custom HTTP headers";
    return descriptor;
}

} // namespace nx::vms::rules
