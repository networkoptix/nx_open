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

using ResultCode = nx::cloud::db::api::ResultCode;

} // namespace api

struct ApiResultCodeDescriptor
{
    using ResultCode = api::ResultCode;

    template<typename... Output>
    static ResultCode getResultCode(
        SystemError::ErrorCode systemErrorCode,
        const nx::network::http::Response* response,
        const nx::network::http::ApiRequestResult& /*fusionRequestResult*/,
        const Output&...)
    {
        if (systemErrorCode != SystemError::noError)
            return systemErrorCodeToResultCode(systemErrorCode);

        if (!response)
            return ResultCode::networkError;

        return getResultCodeFromResponse(*response);
    }

private:
    static ResultCode systemErrorCodeToResultCode(SystemError::ErrorCode systemErrorCode);

    static ResultCode getResultCodeFromResponse(const network::http::Response& response);
};

class ChannelPartnerClient:
    public nx::network::http::GenericApiClient<ApiResultCodeDescriptor>
{
    using base_type = nx::network::http::GenericApiClient<ApiResultCodeDescriptor>;

public:
    /**
     * @param cloudHost Value to be passed in the "cloud-host" header in every request.
     * @param url Base URL of the channel partner service. Actual request URL is constructed as
     * "baseUrl + apiCallPath".
     * TODO: #akolesnikov Get rid of the cloudHost argument when the deployment is fixed.
     */
    ChannelPartnerClient(const std::string& cloudHost, const nx::utils::Url& baseApiUrl);
    ~ChannelPartnerClient();

    void bindSystemToOrganization(
        api::SystemRegistrationRequest data,
        nx::utils::MoveOnlyFunc<void(api::ResultCode, api::SystemRegistrationResponse)> handler);

    /**
     * The request must be authorized using valid user's OAUTH2 token.
     */
    void getSystemUser(
        const std::string& systemId,
        const std::string& email,
        nx::utils::MoveOnlyFunc<void(api::ResultCode, api::User)> handler);

    void getSystemUsers(
        const std::string& systemId,
        nx::utils::MoveOnlyFunc<void(api::ResultCode, std::vector<api::User>)> handler);
};

} // namespace nx::cloud::cps
