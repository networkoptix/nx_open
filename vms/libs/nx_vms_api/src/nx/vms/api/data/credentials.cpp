// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "credentials.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    Credentials, (csv_record)(json)(ubjson)(xml), Credentials_Fields)

Credentials::Credentials(const QString& user, const QString& password):
    user(user),
    password(password)
{
}

Credentials::Credentials(const nx::utils::Url& url):
    Credentials(url.userName(), url.password())
{
}

Credentials Credentials::parseColon(const QString& value, bool hidePassword)
{
    if (const auto colon = value.indexOf(':'); colon >= 0)
    {
        return {value.left(colon),
            hidePassword ? utils::Url::kMaskedPassword : value.mid(colon + 1)};
    }
    return {value, hidePassword ? utils::Url::kMaskedPassword : QString()};
}

bool Credentials::isEmpty() const
{
    return user.isEmpty() && password.isEmpty();
}

bool Credentials::isValid() const
{
    return !user.isEmpty() && !password.isEmpty();
}

QString Credentials::asString() const
{
    return !password.isEmpty() ? (user + ':' + password) : user;
}

} // namespace nx::vms::api
