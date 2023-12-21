// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <string>

#include <nx/cloud/db/api/result_code.h>
#include <nx/network/http/generic_api_client.h>

#include "api/system_data.h"
#include "api/user_data.h"

namespace nx::cloud::cps {

namespace api {

using Result = nx::cloud::db::api::Result;

} // namespace api

struct ApiResultCodeDescriptor
{
    using ResultCode = api::Result;

    template<typename... Output>
    static api::Result getResultCode(
        SystemError::ErrorCode systemErrorCode,
        const nx::network::http::Response* response,
        const nx::network::http::ApiRequestResult& fusionRequestResult,
        const Output&...)
    {
        if (systemErrorCode != SystemError::noError)
            return systemErrorCodeToResultCode(systemErrorCode);

        if (!response)
            return {nx::cloud::db::api::ResultCode::networkError};

        return getResultCodeFromResponse(*response, fusionRequestResult);
    }

private:
    static api::Result systemErrorCodeToResultCode(SystemError::ErrorCode systemErrorCode);

    static api::Result getResultCodeFromResponse(
        const network::http::Response& response,
        const nx::network::http::ApiRequestResult& fusionRequestResult);
};

class ChannelPartnerClient:
    public nx::network::http::GenericApiClient<ApiResultCodeDescriptor>
{
    using base_type = nx::network::http::GenericApiClient<ApiResultCodeDescriptor>;

public:
    ChannelPartnerClient(const nx::utils::Url& baseApiUrl);
    ~ChannelPartnerClient();

    void bindSystemToOrganization(
        api::SystemRegistrationRequest data,
        nx::utils::MoveOnlyFunc<void(api::Result, api::SystemRegistrationResponse)> handler);

    void unbindSystemFromOrganization(
        std::string systemId,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);

    /**
     * The request must be authorized using valid user's OAUTH2 token.
     */
    void getSystemUser(
        const std::string& systemId,
        const std::string& email,
        nx::utils::MoveOnlyFunc<void(api::Result, api::User)> handler);

    /**
     * The request must be authorized using valid user's OAUTH2 token.
     */
    void getSystemUsers(
        const std::string& systemId,
        nx::utils::MoveOnlyFunc<void(api::Result, std::vector<api::User>)> handler);

    void getUserSystems(
        const std::string& email,
        nx::utils::MoveOnlyFunc<void(api::Result, std::vector<api::SystemAllowance>)> handler);
};

} // namespace nx::cloud::cps
