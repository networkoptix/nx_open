// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string_view>

#include <nx/network/http/auth_tools.h>
#include <nx/utils/uuid.h>

namespace nx::network::rest {

struct NX_NETWORK_REST_API AuthSessionSignHeader
{
    nx::Uuid server;
    std::string nonce;
    std::string signature;
};

NX_NETWORK_REST_API std::string generateAuthSessionSign(
    const std::string& username,
    const std::string_view& password,
    const std::string_view& nonce);

NX_NETWORK_REST_API std::optional<std::string> generateAuthSessionHeader(
    const nx::network::http::Credentials& credentials,
    const std::string_view& nonce);

NX_NETWORK_REST_API std::optional<AuthSessionSignHeader> parseAuthSessionHeader(
    const std::string& headerValue);

} // namespace nx::network::rest
