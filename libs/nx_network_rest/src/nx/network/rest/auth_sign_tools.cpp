// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "auth_sign_tools.h"

#include <nx/network/http/custom_headers.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/string.h>

namespace nx::network::rest {

NX_NETWORK_REST_API std::string generateAuthSessionSign(
    const std::string& username,
    std::string_view password,
    std::string_view nonce)
{
    nx::utils::QnCryptographicHash hashCalc((nx::utils::QnCryptographicHash::Sha256));
    hashCalc.addData(username);
    hashCalc.addData(":");
    hashCalc.addData(nonce);
    hashCalc.addData(":");
    hashCalc.addData(password);
    const auto hash = hashCalc.result();
    return nx::utils::toHex(std::string_view(hash.data(), (std::size_t) hash.size()));
}

std::optional<std::string> generateAuthSessionHeader(
    const nx::network::http::Credentials& credentials,
    std::string_view nonce)
{
    if (credentials.username.empty() || !credentials.authToken.isPassword())
        return {};

    const auto signature = generateAuthSessionSign(
        credentials.username, credentials.authToken.value, nonce);

    return nx::utils::buildString(credentials.username, ":", nonce, ":", signature);
}

std::optional<AuthSessionSignHeader> parseAuthSessionHeader(const std::string& headerValue)
{
    auto [tokens, count] = nx::utils::split_n<3>(headerValue, ':');
    if (count < 3)
        return {};
    AuthSessionSignHeader header;
    header.server = nx::Uuid::fromStringSafe(tokens[0]);
    header.nonce = tokens[1];
    header.signature = tokens[2];
    if (header.server.isNull() || header.nonce.empty() || header.signature.empty())
        return {};
    return {std::move(header)};
}

} // namespace nx::network::rest
