// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nx/network/cloud/data/connect_data.h>
#include <nx/network/http/generic_api_client.h>

#include "../../data/result_code.h"
#include "connection_speed.h"
#include "listening_peer.h"
#include "statistics.h"

namespace nx::hpm::api {

struct NX_NETWORK_API ApiResultCodeDescriptor
{
    using ResultCode = api::ResultCode;

    template<typename... Output>
    static ResultCode getResultCode(
        SystemError::ErrorCode systemErrorCode,
        const network::http::Response* response,
        const network::http::ApiRequestResult& apiResult,
        const Output&...)
    {
        if (systemErrorCode != SystemError::noError)
            return systemErrorCodeToResultCode(systemErrorCode);

        if (!response)
            return api::ResultCode::networkError;

        return getResultCodeFromResponse(apiResult, *response);
    }

private:
    static ResultCode systemErrorCodeToResultCode(SystemError::ErrorCode systemErrorCode);

    static ResultCode getResultCodeFromResponse(
        const network::http::ApiRequestResult& apiResult,
        const network::http::Response& response);
};

class NX_NETWORK_API Client:
    public nx::network::http::GenericApiClient<ApiResultCodeDescriptor>
{
    using base_type = nx::network::http::GenericApiClient<ApiResultCodeDescriptor>;

public:
    using ResultCode = ApiResultCodeDescriptor::ResultCode;

    Client(const nx::utils::Url& baseMediatorApiUrl, nx::network::ssl::AdapterFunc adapterFunc);
    ~Client();

    void getListeningPeers(
        const std::string_view& systemId,
        nx::utils::MoveOnlyFunc<void(ResultCode, SystemPeers)> completionHandler);

    std::tuple<ResultCode, SystemPeers> getListeningPeers(const std::string_view& systemId);

    void initiateConnection(
        const ConnectRequest& request,
        nx::utils::MoveOnlyFunc<void(ResultCode, ConnectResponse)> completionHandler);

    void reportUplinkSpeed(
        const PeerConnectionSpeed& connectionSpeed,
        nx::utils::MoveOnlyFunc<void(ResultCode)> completionHandler);

    void resetConnections(
        const std::string_view& systemId,
        nx::utils::MoveOnlyFunc<void(ResultCode, std::vector<std::string>)> completionHandler);

    std::tuple<api::ResultCode, std::vector<std::string>> resetConnections(const std::string_view& systemId);

    void getStatistics(nx::utils::MoveOnlyFunc<void(ResultCode, Statistics)> completionHandler);

    std::tuple<ResultCode, Statistics> getStatistics();

    void getListeningPeersStatistics(
        nx::utils::MoveOnlyFunc<void(ResultCode, ListeningPeerStatistics)> completionHandler);

    std::tuple<ResultCode, ListeningPeerStatistics> getListeningPeersStatistics();

    using base_type::makeAsyncCall;
    using base_type::makeSyncCall;
};

} // namespace nx::hpm::api
