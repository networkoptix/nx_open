// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "jws.h"

#include <nx/reflect/json.h>

namespace nx::network::jws {

namespace detail {

std::size_t estimateSignatureLength(const std::string& algorithm)
{
    std::size_t len = 0;
    if (algorithm == "RS256")
        len = 3072 / 8;
    else if (algorithm == "HS256")
        len = 64;
    else
        len = 256;

    return len * 4 / 3;
}

} // namespace detail

bool verifyToken(const std::string_view encodedToken, const jwk::ParsedKey& key)
{
    const auto headerEndPos = encodedToken.find('.');
    if (headerEndPos == std::string_view::npos)
        return false; // Invalid token format.

    const auto [header, result] = nx::reflect::json::deserialize<Header>(
        nx::utils::fromBase64Url(encodedToken.substr(0, headerEndPos)));
    if (!result)
        return false;

    const auto signedMessageEndPos = encodedToken.find_last_of('.');
    if (signedMessageEndPos == std::string_view::npos)
        return false; // Invalid token format.

    const auto signature = encodedToken.substr(signedMessageEndPos + 1);
    return jwk::verify(encodedToken.substr(0, signedMessageEndPos), signature, header.alg, key);
}

bool verifyToken(const std::string_view encodedToken, const jwk::Key& key)
{
    const auto parsedKey = jwk::parseKey(key);
    if (!parsedKey)
        return false;

    return verifyToken(encodedToken, *parsedKey);
}

} // namespace nx::network::jws
