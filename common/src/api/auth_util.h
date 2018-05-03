#pragma once

#include <nx/network/http/auth_tools.h>

class AuthKey
{
public:
    nx::String username;
    nx::String nonce;
    nx::String response;

    void calcResponse(
        const nx::network::http::AuthToken& authToken,
        nx::network::http::Method::ValueType httpMethod,
        const nx::String& url);
    nx::String toString() const;

    bool parse(const nx::String& str);
    bool verify(
        const nx::network::http::AuthToken& authToken,
        nx::network::http::Method::ValueType httpMethod,
        const nx::String& url);
};
