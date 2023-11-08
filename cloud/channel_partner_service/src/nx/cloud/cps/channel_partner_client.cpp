// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "channel_partner_client.h"

#include <nx/network/http/http_client.h>
#include <nx/network/http/rest/http_rest_client.h>

namespace nx::cloud::cps {

static constexpr char kSystemBindToOrganizationPath[] = "/partners/cloud_systems/";

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
        case StatusCode::forbidden:
            return ResultCode::notAuthorized;

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
    const nx::utils::Url& baseMediatorApiUrl)
    :
    base_type(baseMediatorApiUrl, nx::network::ssl::kDefaultCertificateCheck),
    m_cloudHost(cloudHost)
{
}

ChannelPartnerClient::~ChannelPartnerClient()
{
    pleaseStopSync();
}

void ChannelPartnerClient::bindSystemToOrganization(
    api::SystemRegistrationRequest data,
    nx::utils::MoveOnlyFunc<void(
        ResultCode, api::SystemRegistrationResponse)> completionHandler)
{
    httpClientOptions().addAdditionalHeader("cloud-host", m_cloudHost);
    base_type::template makeAsyncCall<api::SystemRegistrationResponse>(
        nx::network::http::Method::post,
        kSystemBindToOrganizationPath,
        std::move(data),
        std::move(completionHandler));
}

} // namespace nx::cloud::cps
