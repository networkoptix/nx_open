// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server_password_lookup_result.h"

namespace nx::network::http::server {

PasswordLookupResult PlainTextPasswordLookupResultBuilder::build(std::string password)
{
    return PasswordLookupResult{
        PasswordLookupResult::Code::ok,
        PasswordAuthToken(std::move(password))};
}

PasswordLookupResult PlainTextPasswordLookupResultBuilder::build(
    PasswordLookupResult::Code errorCode)
{
    return PasswordLookupResult{
        errorCode,
        AuthToken()};
}

PasswordLookupResult Ha1LookupResultBuilder::build(std::string ha1)
{
    return PasswordLookupResult{
        PasswordLookupResult::Code::ok,
        Ha1AuthToken(std::move(ha1))};
}

PasswordLookupResult Ha1LookupResultBuilder::build(PasswordLookupResult::Code errorCode)
{
    return PasswordLookupResult{
        errorCode,
        AuthToken()};
}

} // namespace nx::network::http::server
