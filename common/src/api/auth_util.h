#pragma once

#include <nx/network/http/auth_tools.h>

class AuthKey
{
public:
    nx::String username;
    nx::String nonce;
    nx::String response;

    void calcResponse(
        const nx_http::AuthToken& authToken,
        nx_http::Method::ValueType httpMethod,
        const nx::String& url);
    nx::String toString() const;

    bool parse(const nx::String& str);
    bool verify(
        const nx_http::AuthToken& authToken,
        nx_http::Method::ValueType httpMethod,
        const nx::String& url);
};
