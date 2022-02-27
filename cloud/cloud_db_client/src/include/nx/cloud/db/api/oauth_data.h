// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace nx::cloud::db::api {

struct IssueTokenRequest
{
    enum class GrantType
    {
        password,
        refreshToken,
        authorizationCode
    };

    enum class ResponseType
    {
        token,
        code
    };

    GrantType grant_type = GrantType::password;
    ResponseType response_type = ResponseType::token;
    std::string client_id;
    std::optional<std::string> scope;
    std::optional<std::chrono::seconds> refresh_token_lifetime;

    std::optional<std::string> password;
    std::optional<std::string> username;
    std::optional<std::string> refresh_token;
    std::optional<std::string> code;
};

enum class TokenType
{
    bearer
};

struct TokenInfo
{
    std::string access_token;
    std::chrono::seconds expires_in;
    std::chrono::milliseconds expires_at; // milliseconds since epoch, UTC
    TokenType token_type;
    std::optional<std::chrono::seconds> prolongation_period;
    std::string scope;
};

struct IssueTokenResponse : public TokenInfo
{
    std::string refresh_token;
    std::optional<std::string> error;
};

struct IssueCodeResponse
{
    std::string access_code;
    std::optional<std::string> error;
};

struct ValidateTokenResponse : public TokenInfo
{
    std::string username;
    std::optional<std::string> vms_user_id;
    std::chrono::seconds time_since_password;
};

} // namespace nx::cloud::db::api
