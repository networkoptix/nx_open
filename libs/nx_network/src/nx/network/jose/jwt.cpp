// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "jwt.h"

#include "jws.h"

namespace nx::network::jwt {

std::optional<std::string> ClaimSet::iss() const
{
    return get<std::string>("iss");
}

void ClaimSet::setIss(const std::string& val)
{
    set("iss", val);
}

std::optional<std::string> ClaimSet::sub() const
{
    return get<std::string>("sub");
}

void ClaimSet::setSub(const std::string& val)
{
    set("sub", val);
}

std::optional<std::string> ClaimSet::aud() const
{
    return get<std::string>("aud");
}

void ClaimSet::setAud(const std::string& val)
{
    set("aud", val);
}

std::optional<std::chrono::seconds> ClaimSet::exp() const
{
    if (const auto val = get<int64_t>("exp"); val.has_value())
        return std::chrono::seconds(*val);
    return std::nullopt;
}

void ClaimSet::setExp(const std::chrono::seconds& val)
{
    set("exp", val.count());
}

std::optional<std::chrono::seconds> ClaimSet::nbf() const
{
    if (const auto val = get<int64_t>("nbf"); val.has_value())
        return std::chrono::seconds(*val);
    return std::nullopt;
}

void ClaimSet::setNbf(const std::chrono::seconds& val)
{
    set("nbf", val.count());
}

std::optional<std::chrono::seconds> ClaimSet::iat() const
{
    if (const auto val = get<int64_t>("iat"); val.has_value())
        return std::chrono::seconds(*val);
    return std::nullopt;
}

void ClaimSet::setIat(const std::chrono::seconds& val)
{
    set("iat", val.count());
}

std::optional<std::string> ClaimSet::jti() const
{
    return get<std::string>("jti");
}

void ClaimSet::setJti(const std::string& val)
{
    set("jti", val);
}

//-------------------------------------------------------------------------------------------------

nx::utils::expected<jws::TokenEncodeResult<ClaimSet>, std::string /*error*/>
    encodeAndSign(ClaimSet claimSet, const jwk::Key& key)
{
    return jws::encodeAndSign<ClaimSet>(kJwtTokenType, std::move(claimSet), key);
}

nx::utils::expected<Token, std::string /*err*/> decodeToken(const std::string_view encoded)
{
    return jws::decodeToken<ClaimSet>(encoded);
}

} // namespace nx::network::jwt
