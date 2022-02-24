// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mediator_api_client.h"

#include <nx/fusion/serialization/json.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/rest/http_rest_client.h>

#include "mediator_api_http_paths.h"

namespace nx::hpm::api {

Client::Client(
    const nx::utils::Url& baseMediatorApiUrl, nx::network::ssl::AdapterFunc adapterFunc):
    base_type(baseMediatorApiUrl, std::move(adapterFunc))
{
}

Client::~Client()
{
    pleaseStopSync();
}

void Client::getListeningPeers(
    const std::string_view& systemId,
    nx::utils::MoveOnlyFunc<void(ResultCode, SystemPeers)> completionHandler)
{
    base_type::template makeAsyncCall<SystemPeers>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(
            kStatisticsSystemPeersPath, {systemId}),
        std::move(completionHandler));
}

std::tuple<Client::ResultCode, SystemPeers> Client::getListeningPeers(
    const std::string_view& systemId)
{
    return base_type::template makeSyncCall<SystemPeers>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(
            kStatisticsSystemPeersPath, {systemId}));
}

void Client::initiateConnection(
    const ConnectRequest& request,
    nx::utils::MoveOnlyFunc<void(ResultCode, ConnectResponse)> completionHandler)
{
    base_type::template makeAsyncCall<ConnectResponse>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(
            kServerSessionsPath, {request.destinationHostName}),
        request,
        std::move(completionHandler));
}

void Client::reportUplinkSpeed(
    const PeerConnectionSpeed& connectionSpeed,
    nx::utils::MoveOnlyFunc<void(ResultCode)> completionHandler)
{
    base_type::template makeAsyncCall<void>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(
            kConnectionSpeedUplinkPathV2, {connectionSpeed.systemId, connectionSpeed.serverId}),
        connectionSpeed.connectionSpeed,
        std::move(completionHandler));
}

void Client::getStatistics(
    nx::utils::MoveOnlyFunc<void(ResultCode, Statistics)> completionHandler)
{
    base_type::template makeAsyncCall<Statistics>(
        nx::network::http::Method::get,
        kStatisticsMetricsPath,
        std::move(completionHandler));
}

std::tuple<ResultCode, Statistics> Client::getStatistics()
{
    return base_type::template makeSyncCall<Statistics>(
        nx::network::http::Method::get,
        kStatisticsMetricsPath);
}

void Client::getListeningPeersStatistics(
    nx::utils::MoveOnlyFunc<void(ResultCode, ListeningPeerStatistics)> completionHandler)
{
    base_type::template makeAsyncCall<ListeningPeerStatistics>(
        nx::network::http::Method::get,
        kStatisticsListeningPeersPath,
        std::move(completionHandler));
}

std::tuple<ResultCode, ListeningPeerStatistics> Client::getListeningPeersStatistics()
{
    return base_type::template makeSyncCall<ListeningPeerStatistics>(
        nx::network::http::Method::get,
        kStatisticsListeningPeersPath);
}

ApiResultCodeDescriptor::ResultCode ApiResultCodeDescriptor::systemErrorCodeToResultCode(
    SystemError::ErrorCode systemErrorCode)
{
    switch (systemErrorCode)
    {
        case SystemError::noError:
            return api::ResultCode::ok;
        case SystemError::timedOut:
            return api::ResultCode::timedOut;
        default:
            return api::ResultCode::networkError;
    }
}

ApiResultCodeDescriptor::ResultCode ApiResultCodeDescriptor::getResultCodeFromResponse(
    const network::http::Response& response)
{
    using namespace network::http;

    if (StatusCode::isSuccessCode(response.statusLine.statusCode))
        return api::ResultCode::ok;

    switch (response.statusLine.statusCode)
    {
        case StatusCode::unauthorized:
            return api::ResultCode::notAuthorized;

        case StatusCode::badRequest:
            return api::ResultCode::badRequest;

        case StatusCode::notFound:
            return api::ResultCode::notFound;

        default:
            return api::ResultCode::otherLogicError;
    }
}

} // namespace nx::hpm::api
