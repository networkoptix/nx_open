// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/utils/buffer.h>

#include "../auth_tools.h"

namespace nx::network::http::server {

struct NX_NETWORK_API PasswordLookupResult
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

class NX_NETWORK_API PlainTextPasswordLookupResultBuilder
{
public:
    static PasswordLookupResult build(std::string password);
    static PasswordLookupResult build(PasswordLookupResult::Code errorCode);
};

class NX_NETWORK_API Ha1LookupResultBuilder
{
public:
    static PasswordLookupResult build(std::string ha1);
    static PasswordLookupResult build(PasswordLookupResult::Code errorCode);
};

} // namespace nx::network::http::server
