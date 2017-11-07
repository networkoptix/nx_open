#include "auth_util.h"

#include <nx/network/app_info.h>

nx::String buildAuthKey(
    const nx::String& url,
    const nx_http::Credentials& credentials,
    const nx::String& nonce)
{
    const auto ha1 =
        credentials.authToken.type == nx_http::AuthTokenType::ha1
        ? credentials.authToken.value.toUtf8()
        : nx_http::calcHa1(
            credentials.username,
            nx::network::AppInfo::realm(),
            credentials.authToken.value);
    const auto ha2 = nx_http::calcHa2(nx_http::Method::get, url);
    const auto response = nx_http::calcResponse(ha1, nonce, ha2);
    return lm("%1:%2:%3").args(credentials.username, nonce, response).toUtf8().toBase64();
}
