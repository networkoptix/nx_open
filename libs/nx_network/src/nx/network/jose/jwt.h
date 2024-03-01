// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string>

#include <nx/reflect/instrument.h>
#include <nx/reflect/json/object.h>

#include "jws.h"

namespace nx::network::jwt {

static constexpr char kJwtTokenType[] = "JWT";

/**
 * JWT claims set. Field names follow the claim names used in the RFC 7519.
 */
class NX_NETWORK_API ClaimSet:
    public nx::reflect::json::Object
{
public:
    std::optional<std::string> iss() const;
    void setIss(const std::string& val);

    std::optional<std::string> sub() const;
    void setSub(const std::string& val);

    std::optional<std::string> aud() const;
    void setAud(const std::string& val);

    std::optional<std::chrono::seconds> exp() const;
    void setExp(const std::chrono::seconds& val);

    std::optional<std::chrono::seconds> nbf() const;
    void setNbf(const std::chrono::seconds& val);

    std::optional<std::chrono::seconds> iat() const;
    void setIat(const std::chrono::seconds& val);

    std::optional<std::string> jti() const;
    void setJti(const std::string& val);
};

/**
 * JWT token as defined in RFC 7519.
 */
using Token = jws::Token<ClaimSet>;

//-------------------------------------------------------------------------------------------------

/**
 * Encode and sign token. It is a wrapper around jws::encodeAndSign().
 */
NX_NETWORK_API nx::utils::expected<jws::TokenEncodeResult<ClaimSet>, std::string /*error*/>
    encodeAndSign(ClaimSet claimSet, const jwk::Key& key);

/**
 * Decode token without verifying its signature. It is a wrapper around jws::decodeToken().
 */
NX_NETWORK_API nx::utils::expected<Token, std::string /*err*/> decodeToken(
    const std::string_view encoded);

using jws::verifyToken;

} // namespace nx::network::jwt
