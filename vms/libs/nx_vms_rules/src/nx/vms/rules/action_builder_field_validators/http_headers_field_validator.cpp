// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "http_headers_field_validator.h"

#include "../action_builder_fields/http_headers_field.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult HttpHeadersFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* /*context*/) const
{
    auto httpHeadersField = dynamic_cast<const HttpHeadersField*>(field);
    if (!NX_ASSERT(httpHeadersField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    const auto headers = httpHeadersField->value();
    if (headers.empty())
        return {};

    int valid{};
    int invalid{};
    for (const auto& [key, _]: httpHeadersField->value())
    {
        if (key.isEmpty() || key.contains(QRegularExpression((R"(\s)"))))
            ++invalid;
        else
            ++valid;
    }

    if (valid > 0 && invalid > 0)
        return {QValidator::State::Intermediate, tr("Some of the headers are not valid", "", invalid)};

    if (invalid > 0)
        return {QValidator::State::Invalid, tr("All the headers are not valid", "", invalid)};

    return {};
}

} // namespace nx::vms::rules
