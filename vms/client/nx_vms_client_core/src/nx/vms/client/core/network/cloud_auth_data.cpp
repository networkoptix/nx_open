// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_auth_data.h"

#include <nx/network/jose/jwt.h>

namespace nx::vms::client::core {

static constexpr std::string_view kTokenPrefix = "nxcdb-";

std::string usernameFromToken(const std::string& value)
{
    std::string_view token = value;

    if (token.starts_with(kTokenPrefix))
        token.remove_prefix(kTokenPrefix.size());

    if (auto result = nx::network::jws::decodeToken<nx::network::jwt::ClaimSet>(token))
        return result->payload.sub().value_or("");

    return {};
}

} // namespace nx::vms::client::core
