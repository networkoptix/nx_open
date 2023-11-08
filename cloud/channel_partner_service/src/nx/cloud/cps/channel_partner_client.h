// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <string>

#include <nx/cloud/cps/api/system_data.h>
#include <nx/cloud/db/api/result_code.h>
#include <nx/network/http/generic_api_client.h>

namespace nx::cloud::cps {

struct ApiResultCodeDescriptor
{
    using ResultCode = nx::cloud::db::api::ResultCode;

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

    using ResultCode = ApiResultCodeDescriptor::ResultCode;

public:
    ChannelPartnerClient(const std::string& cloudHost, const nx::utils::Url& url);
    ~ChannelPartnerClient();

    void bindSystemToOrganization(
        api::SystemRegistrationRequest data,
        nx::utils::MoveOnlyFunc<void(
            ResultCode, api::SystemRegistrationResponse)> completionHandler);

private:
    const std::string m_cloudHost;
};

} // namespace nx::cloud::cps
