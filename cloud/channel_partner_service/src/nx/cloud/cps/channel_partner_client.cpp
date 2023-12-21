// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "channel_partner_client.h"

#include <nx/network/http/http_client.h>
#include <nx/network/http/rest/http_rest_client.h>

#include "api/request_paths.h"

namespace nx::cloud::cps {

api::Result ApiResultCodeDescriptor::systemErrorCodeToResultCode(
    SystemError::ErrorCode systemErrorCode)
{
    switch (systemErrorCode)
    {
        case SystemError::noError:
            return { nx::cloud::db::api::ResultCode::ok };
        default:
            return { nx::cloud::db::api::ResultCode::networkError };
    }
}

api::Result ApiResultCodeDescriptor::getResultCodeFromResponse(
    const network::http::Response& response,
    const nx::network::http::ApiRequestResult& fusionRequestResult)
{
    using namespace network::http;

    if (StatusCode::isSuccessCode(response.statusLine.statusCode))
        return { nx::cloud::db::api::ResultCode::ok };

    const auto it = fusionRequestResult.find("detail");
    std::string error = it != fusionRequestResult.end() && !it->second.empty()
        ? it->second : fusionRequestResult.getErrorText();

    switch (response.statusLine.statusCode)
    {
        case StatusCode::unauthorized:
            return {nx::cloud::db::api::ResultCode::notAuthorized, error};

        case StatusCode::forbidden:
            return {nx::cloud::db::api::ResultCode::forbidden, error};

        case StatusCode::notFound:
            return {nx::cloud::db::api::ResultCode::notFound, error};

        default:
            if (response.statusLine.statusCode / 100 * 100 == StatusCode::badRequest)
                return {nx::cloud::db::api::ResultCode::badRequest, error};
            return {nx::cloud::db::api::ResultCode::unknownError, error};
    }
}

// -----ChannelPartnerClient ------

ChannelPartnerClient::ChannelPartnerClient(const nx::utils::Url& baseApiUrl)
    :
    base_type(baseApiUrl, nx::network::ssl::kDefaultCertificateCheck)
{
    // TODO: remove it when CPS service stop requires it.
    httpClientOptions().addAdditionalHeader("cloud-host", baseApiUrl.host().toStdString());
}

ChannelPartnerClient::~ChannelPartnerClient()
{
    pleaseStopSync();
}

void ChannelPartnerClient::bindSystemToOrganization(
    api::SystemRegistrationRequest data,
    nx::utils::MoveOnlyFunc<void(api::Result, api::SystemRegistrationResponse)> handler)
{
    base_type::template makeAsyncCall<api::SystemRegistrationResponse>(
        nx::network::http::Method::post,
        api::kSystemBindToOrganizationPath,
        std::move(data),
        std::move(handler));
}

void ChannelPartnerClient::unbindSystemFromOrganization(
    std::string systemId,
    nx::utils::MoveOnlyFunc<void(api::Result)> handler)
{
    base_type::template makeAsyncCall<void>(
        nx::network::http::Method::delete_,
        nx::network::http::rest::substituteParameters(
            api::kUnbindSystemFromOrganizationPath, {systemId}),
        std::move(handler));
}

void ChannelPartnerClient::getSystemUser(
    const std::string& systemId,
    const std::string& email,
    nx::utils::MoveOnlyFunc<void(api::Result, api::User)> handler)
{
    base_type::template makeAsyncCall<api::User>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(
            api::kInternalSystemUserPath, {systemId, email}),
        std::move(handler));
}

void ChannelPartnerClient::getSystemUsers(
    const std::string& systemId,
    nx::utils::MoveOnlyFunc<void(api::Result, std::vector<api::User>)> handler)
{
    base_type::template makeAsyncCall<std::vector<api::User>>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(api::kInternalSystemUsersPath, {systemId}),
        std::move(handler));
}

void ChannelPartnerClient::getUserSystems(
    const std::string& email,
    nx::utils::MoveOnlyFunc<void(api::Result, std::vector<api::SystemAllowance>)> handler)
{
    base_type::template makeAsyncCall<std::vector<api::SystemAllowance>>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(api::kInternalUserSystemsPath, {email}),
        std::move(handler));
}

} // namespace nx::cloud::cps
