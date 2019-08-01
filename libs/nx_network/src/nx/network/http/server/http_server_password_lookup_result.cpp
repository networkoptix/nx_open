#include "http_server_password_lookup_result.h"

namespace nx::network::http::server {

PasswordLookupResult PlainTextPasswordLookupResultBuilder::build(nx::String password)
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

PasswordLookupResult Ha1LookupResultBuilder::build(nx::String ha1)
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
