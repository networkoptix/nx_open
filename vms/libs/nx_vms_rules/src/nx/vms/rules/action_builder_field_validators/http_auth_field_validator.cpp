// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_auth_field_validator.h"

#include <nx/network/http/http_types.h>

#include "../action_builder_fields/http_auth_field.h"
#include "../strings.h"

namespace nx::vms::rules {

ValidationResult HttpAuthFieldValidator::validity(
    const Field* field, const Rule* /*rule*/, common::SystemContext* /*context*/) const
{
    auto httpAuthField = dynamic_cast<const HttpAuthField*>(field);
    if (!NX_ASSERT(httpAuthField))
        return {QValidator::State::Invalid, Strings::invalidFieldType()};

    switch (httpAuthField->authType())
    {
        case network::http::AuthType::authBasicAndDigest:
            break;
        case network::http::AuthType::authDigest:
        case network::http::AuthType::authBasic:
            if (httpAuthField->login().isEmpty() || httpAuthField->password().isEmpty())
            {
                return {
                    QValidator::State::Invalid,
                    tr("User & password fields should be filled in case of basic or digest auth method selected")};
            }

            break;
        case network::http::AuthType::authBearer:
            if (httpAuthField->token().isEmpty())
            {
                return {
                    QValidator::State::Invalid,
                    tr("Token field should be filled in case of bearer auth type selected")};
            }

            break;
        default:
            const auto alertMessage = QString{"'%1' - is unexpected auth type"}.arg(
                QString::fromStdString(nx::reflect::toString(httpAuthField->authType())));
            NX_ASSERT(false, alertMessage);
            return {QValidator::State::Invalid, alertMessage};
    }

    return {};
}

} // namespace nx::vms::rules
