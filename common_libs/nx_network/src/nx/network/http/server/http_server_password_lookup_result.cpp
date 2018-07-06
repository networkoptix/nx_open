#include "http_server_password_lookup_result.h"

namespace nx {
namespace network {
namespace http {
namespace server {

//-------------------------------------------------------------------------------------------------

boost::optional<nx::String> PasswordLookupResult::password() const
{
    if (type == DataType::password)
        return data;
    return boost::none;
}

boost::optional<nx::String> PasswordLookupResult::ha1() const
{
    if (type == DataType::ha1)
        return data;
    return boost::none;
}

//-------------------------------------------------------------------------------------------------

PasswordLookupResult PlainTextPasswordLookupResultBuilder::build(nx::String password)
{
    return PasswordLookupResult{
        PasswordLookupResult::Code::ok,
        PasswordLookupResult::DataType::password,
        std::move(password)};
}

PasswordLookupResult PlainTextPasswordLookupResultBuilder::build(PasswordLookupResult::Code errorCode)
{
    return PasswordLookupResult{
        errorCode,
        PasswordLookupResult::DataType::password,
        nx::String()};
}

PasswordLookupResult Ha1LookupResultBuilder::build(nx::String ha1)
{
    return PasswordLookupResult{
        PasswordLookupResult::Code::ok,
        PasswordLookupResult::DataType::ha1,
        std::move(ha1)};
}

PasswordLookupResult Ha1LookupResultBuilder::build(PasswordLookupResult::Code errorCode)
{
    return PasswordLookupResult{
        errorCode,
        PasswordLookupResult::DataType::password,
        nx::String()};
}

} // namespace server
} // namespace nx
} // namespace network
} // namespace http
