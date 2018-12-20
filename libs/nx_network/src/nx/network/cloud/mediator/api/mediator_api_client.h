#pragma once

#include <memory>
#include <vector>

#include <nx/network/cloud/data/connect_data.h>
#include <nx/network/http/generic_api_client.h>

#include "listening_peer.h"
#include "../../data/result_code.h"

namespace nx {
namespace hpm {
namespace api {

class NX_NETWORK_API Client:
    public nx::network::http::GenericApiClient<Client>
{
    using base_type = nx::network::http::GenericApiClient<Client>;

public:
    using ResultCode = api::ResultCode;

    Client(const nx::utils::Url& baseMediatorApiUrl);
    ~Client();

    void getListeningPeers(
        nx::utils::MoveOnlyFunc<void(ResultCode, ListeningPeers)> completionHandler);

    std::tuple<ResultCode, ListeningPeers> getListeningPeers();

    void initiateConnection(
        const ConnectRequest& request,
        nx::utils::MoveOnlyFunc<void(ResultCode, ConnectResponse)> completionHandler);

    template <typename... Output>
    static ResultCode getResultCode(
        SystemError::ErrorCode systemErrorCode,
        const network::http::Response* response,
        const Output&...)
    {
        if (systemErrorCode != SystemError::noError)
            return systemErrorCodeToResultCode(systemErrorCode);

        if (!response)
            return api::ResultCode::networkError;

        return getResultCodeFromResponse(*response);
    }

private:
    static ResultCode systemErrorCodeToResultCode(
        SystemError::ErrorCode systemErrorCode);

    static ResultCode getResultCodeFromResponse(
        const network::http::Response& response);
};

} // namespace api
} // namespace hpm
} // namespace nx
