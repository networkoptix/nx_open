// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include "result_code.h"
#include "oauth_data.h"

#include <nx/network/http/http_types.h>
#include <nx/utils/move_only_func.h>

namespace nx::cloud::db::api {

class OauthManager
{
public:
    static constexpr std::string_view kTokenPrefix = "nxcdb-";
    static constexpr std::string_view k2faRequiredError = "second_factor_required";
    static constexpr std::string_view k2faDisabledForUserError = "2fa_disabled_for_the_user";

    virtual ~OauthManager() = default;

    virtual void issueToken(
        const IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(ResultCode, IssueTokenResponse)> completionHandler) = 0;

    virtual void issueAuthorizationCode(
        const IssueTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(ResultCode, IssueCodeResponse)> completionHandler) = 0;

    virtual void validateToken(
        const std::string& token,
        nx::utils::MoveOnlyFunc<void(ResultCode, ValidateTokenResponse)> completionHandler) = 0;

    virtual void deleteToken(
        const std::string& token, nx::utils::MoveOnlyFunc<void(ResultCode)> completionHandler) = 0;

    virtual void deleteTokens(
        const std::string& clientId,
        nx::utils::MoveOnlyFunc<void(ResultCode)> completionHandler) = 0;

    virtual void logout(nx::utils::MoveOnlyFunc<void(ResultCode)> completionHandler) = 0;

    virtual void issueStunToken(const IssueStunTokenRequest& request,
        nx::utils::MoveOnlyFunc<void(ResultCode, IssueStunTokenResponse)> completionHandler) = 0;
};

} // namespace nx::cloud::db::api
