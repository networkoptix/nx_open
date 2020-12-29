#pragma once

#include <nx/network/buffer.h>

#include "../auth_tools.h"

namespace nx::network::http::server {

struct PasswordLookupResult
{
    enum class Code
    {
        ok,
        notFound,
        otherError,
    };

    Code code;
    AuthToken authToken;
};

class PlainTextPasswordLookupResultBuilder
{
public:
    static PasswordLookupResult build(nx::String password);
    static PasswordLookupResult build(PasswordLookupResult::Code errorCode);
};

class Ha1LookupResultBuilder
{
public:
    static PasswordLookupResult build(nx::String ha1);
    static PasswordLookupResult build(PasswordLookupResult::Code errorCode);
};

} // namespace nx::network::http::server
