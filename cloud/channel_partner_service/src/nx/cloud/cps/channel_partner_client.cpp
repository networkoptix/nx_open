// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "channel_partner_client.h"

#include <nx/network/http/http_client.h>
#include <nx/network/http/rest/http_rest_client.h>

#include "api/request_paths.h"

namespace nx::cloud::cps {

ApiResultCodeDescriptor::ResultCode ApiResultCodeDescriptor::systemErrorCodeToResultCode(
    SystemError::ErrorCode systemErrorCode)
{
    switch (systemErrorCode)
    {
        case SystemError::noError:
            return ResultCode::ok;
        default:
            return ResultCode::networkError;
    }
}

ApiResultCodeDescriptor::ResultCode ApiResultCodeDescriptor::getResultCodeFromResponse(
    const network::http::Response& response)
{
    using namespace network::http;

    if (StatusCode::isSuccessCode(response.statusLine.statusCode))
        return ResultCode::ok;

    switch (response.statusLine.statusCode)
    {
        case StatusCode::unauthorized:
            return ResultCode::notAuthorized;

        case StatusCode::forbidden:
            return ResultCode::forbidden;

        case StatusCode::notFound:
            return ResultCode::notFound;

        default:
            if (response.statusLine.statusCode / 100 * 100 == StatusCode::badRequest)
                return ResultCode::badRequest;
            return ResultCode::unknownError;
    }
}

// -----ChannelPartnerClient ------

ChannelPartnerClient::ChannelPartnerClient(
    const std::string& cloudHost,
    const nx::utils::Url& baseApiUrl)
    :
    base_type(baseApiUrl, nx::network::ssl::kDefaultCertificateCheck)
{
    httpClientOptions().addAdditionalHeader("cloud-host", cloudHost);
}

ChannelPartnerClient::~ChannelPartnerClient()
{
    pleaseStopSync();
}

void ChannelPartnerClient::bindSystemToOrganization(
    api::SystemRegistrationRequest data,
    nx::utils::MoveOnlyFunc<void(api::ResultCode, api::SystemRegistrationResponse)> handler)
{
    base_type::template makeAsyncCall<api::SystemRegistrationResponse>(
        nx::network::http::Method::post,
        api::kSystemBindToOrganizationPath,
        std::move(data),
        std::move(handler));
}

void ChannelPartnerClient::getSystemUser(
    const std::string& systemId,
    const std::string& email,
    nx::utils::MoveOnlyFunc<void(api::ResultCode, api::User)> handler)
{
    base_type::template makeAsyncCall<api::User>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(
            api::kInternalSystemUserPath, {systemId, email}),
        std::move(handler));
}

} // namespace nx::cloud::cps
