// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_auth_data.h"

#include <optional>

#include <nx/cloud/db/client/data/access_scope.h>
#include <nx/network/jose/jwt.h>

namespace nx::vms::client::core {

static constexpr std::string_view kTokenPrefix = "nxcdb-";

namespace {

std::optional<nx::network::jwt::ClaimSet> decodeClaims(const std::string& value)
{
    std::string_view token = value;

    if (token.starts_with(kTokenPrefix))
        token.remove_prefix(kTokenPrefix.size());

    if (auto result = nx::network::jws::decodeToken<nx::network::jwt::ClaimSet>(token))
        return result->payload;

    return std::nullopt;
}

} // namespace

std::string usernameFromToken(const std::string& value)
{
    if (const auto claims = decodeClaims(value))
        return claims->sub().value_or("");

    return {};
}

bool isSsoSession(const std::string& refreshToken)
{
    const auto claims = decodeClaims(refreshToken);
    if (!claims)
        return false;

    const auto scope = nx::cloud::db::api::AccessScope::buildAccessScope(claims->aud().value_or(""));
    return scope
        && scope->attributes().count(nx::cloud::db::api::AccessScope::kSsoOrganizationId) > 0;
}

} // namespace nx::vms::client::core
