#include "auth_util.h"

#include <nx/network/app_info.h>

void AuthKey::calcResponse(
    const nx::network::http::AuthToken& authToken,
    nx::network::http::Method::ValueType httpMethod,
    const nx::String& url)
{
    const auto ha1 =
        authToken.type == nx::network::http::AuthTokenType::ha1
        ? authToken.value
        : nx::network::http::calcHa1(
            username,
            nx::network::AppInfo::realm().toUtf8(),
            authToken.value);
    const auto ha2 = nx::network::http::calcHa2(httpMethod, url);
    response = nx::network::http::calcResponse(ha1, nonce, ha2);
}

nx::String AuthKey::toString() const
{
    return lm("%1:%2:%3").args(username, nonce, response).toUtf8().toBase64();
}

bool AuthKey::parse(const nx::String& str)
{
    const auto parts = nx::String::fromBase64(str).split(':');
    if (parts.size() < 3)
        return false;

    username = parts[0];
    nonce = parts[1];
    response = parts[2];
    return true;
}

bool AuthKey::verify(
    const nx::network::http::AuthToken& authToken,
    nx::network::http::Method::ValueType httpMethod,
    const nx::String& url)
{
    AuthKey key2(*this);
    key2.calcResponse(authToken, httpMethod, url);

    return response == key2.response;
}
