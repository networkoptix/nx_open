// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_method_field_validator.h"

#include <nx/network/http/http_types.h>

#include "../action_builder_fields/http_method_field.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult HttpMethodFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* /*context*/) const
{
    auto httpMethodField = dynamic_cast<const HttpMethodField*>(field);
    if (!NX_ASSERT(httpMethodField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    static const QSet<nx::String> kHttpMethods{
        network::http::Method::get,
        network::http::Method::post,
        network::http::Method::put,
        network::http::Method::patch,
        network::http::Method::delete_};

    const auto method = httpMethodField->value().trimmed().toUpper();

    if (method.isEmpty() || kHttpMethods.contains(method)) //< Empty string is 'Auto' method type.
        return {};

    return {
        QValidator::State::Invalid,
        tr("HTTP Method should be known")
    };
}

} // namespace nx::vms::rules
