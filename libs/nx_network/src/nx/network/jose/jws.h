// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/reflect/instrument.h>
#include <nx/utils/base64.h>

#include <nx/utils/std_string_utils.h>

#include "jwk.h"

namespace nx::network::jws {

namespace detail {

NX_NETWORK_API std::size_t estimateSignatureLength(const jwk::Key& key);

} // namespace detail

/**
 * JWS header.
 */
struct Header
{
    std::string typ;
    std::string alg;
    std::optional<std::string> kid;

    bool operator==(const Header&) const = default;
};

NX_REFLECTION_INSTRUMENT(Header, (typ)(alg)(kid))

/**
 * JWS token as defined in RFC 7515.
 */
template<typename Payload>
struct Token
{
    /** Protected header. */
    Header header;
    // NOTE: unprotected header is not supported.

    /** Token payload. MUST be JSON serializable. */
    Payload payload;

    bool operator==(const Token<Payload>&) const = default;
};

template<typename Payload>
struct TokenEncodeResult
{
    Token<Payload> token;
    std::string encodedAndSignedToken;
};

/**
 * Encodes and signs token as defined by
 * https://datatracker.ietf.org/doc/html/rfc7515#appendix-A.1.
 * The token header is generated within this function to set signing 'alg' properly.
 * @param tokenType A token type. E.g., "JWT" for rfc7519 tokens.
 * @return Encoded and signed token.
 */
template<typename Payload>
nx::utils::expected<TokenEncodeResult<std::decay_t<Payload>>, std::string /*error*/>
    encodeAndSign(const std::string& tokenType, Payload&& tokenPayload, const jwk::Key& key)
{
    Token<std::decay_t<Payload>> token;
    token.payload = std::forward<Payload>(tokenPayload);
    token.header.typ = tokenType;
    auto alg = jwk::getAlgorithmForSigning(key);
    if (!alg)
        return nx::utils::unexpected<std::string>("Cannot sign with the given key: " + alg.error());
    token.header.alg = *alg;
    token.header.kid = key.kid();

    const auto serializedHeader = nx::reflect::json::serialize(token.header);
    const auto serializedPayload = nx::reflect::json::serialize(token.payload);

    std::string encodedToken;
    encodedToken.reserve((serializedHeader.size() / 3 + 1) * 4
        + 1 // '.'
        + (serializedPayload.size() / 3 + 1) * 4
        + 1 // '.'
        + detail::estimateSignatureLength(key)
        + 1 // null terminator
    );

    nx::utils::toBase64Url(serializedHeader, &encodedToken);
    encodedToken += ".";
    nx::utils::toBase64Url(serializedPayload, &encodedToken);
    // TODO: pass alg from token.header?
    auto signResult = jwk::sign(encodedToken, key);
    if (!signResult)
        return nx::utils::unexpected<std::string>("Signing error: " + signResult.error());

    encodedToken += ".";
    encodedToken += signResult->signature;

    return TokenEncodeResult<std::decay_t<Payload>>{
        .token = std::move(token), .encodedAndSignedToken = encodedToken};
}

/**
 * Decode token header and payload without verifying.
 * Note: the token signature is ommited by this function. Use verifyToken to verify the signature.
 */
template<typename Payload>
nx::utils::expected<Token<Payload>, std::string /*err*/> decodeToken(const std::string_view encoded)
{
    auto [parts, count] = nx::utils::split_n<3>(encoded, '.');
    if (count != 3)
        return nx::utils::unexpected<std::string>("Invalid format");

    Token<Payload> token;
    nx::reflect::DeserializationResult result;

    const std::string headerJson = nx::utils::fromBase64Url(parts[0]);
    std::tie(token.header, result) = nx::reflect::json::deserialize<Header>(headerJson);
    if (!result)
        return nx::utils::unexpected<std::string>("Error parsing token header: " + result.errorDescription);

    const std::string payloadJson = nx::utils::fromBase64Url(parts[1]);
    std::tie(token.payload, result) = nx::reflect::json::deserialize<Payload>(payloadJson);
    if (!result)
        return nx::utils::unexpected<std::string>("Error parsing token payload: " + result.errorDescription);

    return token;
}

NX_NETWORK_API bool verifyToken(const std::string_view encodedToken, const jwk::Key& key);

} // namespace nx::network::jws
