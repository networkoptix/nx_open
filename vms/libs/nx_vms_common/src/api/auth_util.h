// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/auth_tools.h>
#include <nx/string.h>

class NX_VMS_COMMON_API AuthKey
{
public:
    nx::String username;
    nx::String nonce;
    nx::String response;

    void calcResponse(
        const nx::network::http::AuthToken& authToken,
        nx::network::http::Method httpMethod,
        const nx::String& url);
    nx::String toString() const;

    bool parse(const nx::String& str);
    bool verify(
        const nx::network::http::AuthToken& authToken,
        nx::network::http::Method httpMethod,
        const nx::String& url);
};
